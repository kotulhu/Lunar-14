// Тесты VirtualFs: normalize, readFile, writeFile, слой изменений.
#include "assert.h"
#include "registry.h"

#include "core/Host.h"
#include "core/VirtualFs.h"

// Создаёт тестовый хост с файловой системой:
//   /
//   └── home/
//       └── user/
//           ├── file.txt   (содержимое "hello world")
//           └── sub/       (пустой каталог)
static Host makeVfsHost() {
    FsNode file;
    file.is_dir = false;
    file.content = "hello world";

    FsNode sub;
    sub.is_dir = true;

    FsNode user_dir;
    user_dir.is_dir = true;
    user_dir.children["file.txt"] = file;
    user_dir.children["sub"] = sub;

    FsNode home_dir;
    home_dir.is_dir = true;
    home_dir.children["user"] = user_dir;

    FsNode root;
    root.is_dir = true;
    root.children["home"] = home_dir;

    Host h;
    h.name = "vfs-test";
    h.ip = "0.0.0.1";
    h.root = std::move(root);
    return h;
}

// ── normalize ────────────────────────────────────────────────────────────────

TEST(vfs_normalize_dot) {
    // "." не меняет текущий каталог.
    EXPECT_EQ(VirtualFs::normalize("/home/user", "."), "/home/user");
    EXPECT_EQ(VirtualFs::normalize("/", "."), "/");
}

TEST(vfs_normalize_empty) {
    // Пустой путь = остаёмся на месте.
    EXPECT_EQ(VirtualFs::normalize("/home/user", ""), "/home/user");
    EXPECT_EQ(VirtualFs::normalize("/", ""), "/");
}

TEST(vfs_normalize_double_slash) {
    // Двойной слэш схлопывается в одинарный.
    EXPECT_EQ(VirtualFs::normalize("/home", "//user"), "/user");
    EXPECT_EQ(VirtualFs::normalize("/", "//"), "/");
}

TEST(vfs_normalize_absolute) {
    // Абсолютный путь игнорирует current_dir.
    EXPECT_EQ(VirtualFs::normalize("/home/user", "/etc"), "/etc");
    EXPECT_EQ(VirtualFs::normalize("/home/user", "/"), "/");
    EXPECT_EQ(VirtualFs::normalize("/anywhere", "/a/b/c"), "/a/b/c");
}

TEST(vfs_normalize_home) {
    EXPECT_EQ(VirtualFs::normalize("/home/hacker/docs", "~"), "/home/hacker");
    EXPECT_EQ(VirtualFs::normalize("/home/guest", "~/mail.txt"),
              "/home/guest/mail.txt");
}

TEST(vfs_normalize_dotdot) {
    // ".." поднимается на уровень вверх.
    EXPECT_EQ(VirtualFs::normalize("/home/user", ".."), "/home");
    EXPECT_EQ(VirtualFs::normalize("/home", ".."), "/");
    // ".." от корня не выходит за "/".
    EXPECT_EQ(VirtualFs::normalize("/", ".."), "/");
}

TEST(vfs_normalize_dotdot_chain) {
    // Несколько ".." подряд.
    EXPECT_EQ(VirtualFs::normalize("/home/user/sub", "../.."), "/home");
    EXPECT_EQ(VirtualFs::normalize("/a/b/c/d", "../../.."), "/a");
}

// ── readFile ─────────────────────────────────────────────────────────────────

TEST(vfs_readFile_exists) {
    Host h = makeVfsHost();
    VirtualFs::clearChanges();
    bool ok = false;
    std::string content = VirtualFs::readFile(h, "/home/user/file.txt", ok);
    EXPECT_TRUE(ok);
    EXPECT_EQ(content, "hello world");
}

TEST(vfs_readFile_missing) {
    // Несуществующий файл → ok=false.
    Host h = makeVfsHost();
    bool ok = true;
    VirtualFs::readFile(h, "/home/user/nope.txt", ok);
    EXPECT_FALSE(ok);
}

TEST(vfs_readFile_on_dir) {
    // Каталог нельзя читать как файл → ok=false.
    Host h = makeVfsHost();
    bool ok = true;
    VirtualFs::readFile(h, "/home/user", ok);
    EXPECT_FALSE(ok);
}

// ── writeFile ────────────────────────────────────────────────────────────────

TEST(vfs_writeFile_new_file) {
    // Новый файл попадает в overlay и читается обратно.
    Host h = makeVfsHost();
    VirtualFs::clearChanges();

    bool written = VirtualFs::writeFile(h, "/home/user/new.txt", "new content");
    EXPECT_TRUE(written);

    bool ok = false;
    EXPECT_EQ(VirtualFs::readFile(h, "/home/user/new.txt", ok), "new content");
    EXPECT_TRUE(ok);

    VirtualFs::clearChanges();
}

TEST(vfs_writeFile_overrides_base) {
    // Запись поверх базового файла — читается изменённая версия.
    Host h = makeVfsHost();
    VirtualFs::clearChanges();

    VirtualFs::writeFile(h, "/home/user/file.txt", "modified");
    bool ok = false;
    EXPECT_EQ(VirtualFs::readFile(h, "/home/user/file.txt", ok), "modified");
    EXPECT_TRUE(ok);

    VirtualFs::clearChanges();
}

TEST(vfs_writeFile_bad_parent) {
    // Несуществующий родительский каталог → запись запрещена.
    Host h = makeVfsHost();
    VirtualFs::clearChanges();

    bool written = VirtualFs::writeFile(h, "/home/nobody/file.txt", "x");
    EXPECT_FALSE(written);

    VirtualFs::clearChanges();
}

TEST(vfs_writeFile_onto_dir) {
    // Нельзя записать файл по пути существующего каталога.
    Host h = makeVfsHost();
    VirtualFs::clearChanges();

    bool written = VirtualFs::writeFile(h, "/home/user/sub", "x");
    EXPECT_FALSE(written);

    VirtualFs::clearChanges();
}

// ── Слой изменений ───────────────────────────────────────────────────────────

TEST(vfs_changes_layer) {
    // Слой пуст после clearChanges; пополняется при записи.
    Host h = makeVfsHost();
    VirtualFs::clearChanges();
    EXPECT_TRUE(VirtualFs::changes().empty());

    VirtualFs::writeFile(h, "/home/user/x.txt", "x");
    EXPECT_FALSE(VirtualFs::changes().empty());

    VirtualFs::clearChanges();
    EXPECT_TRUE(VirtualFs::changes().empty());
}

TEST(vfs_changes_isolated_by_host) {
    // Изменения хранятся раздельно по имени хоста.
    Host h1 = makeVfsHost();
    h1.name = "host-a";

    Host h2 = makeVfsHost();
    h2.name = "host-b";

    VirtualFs::clearChanges();
    VirtualFs::writeFile(h1, "/home/user/file.txt", "from-a");
    VirtualFs::writeFile(h2, "/home/user/file.txt", "from-b");

    bool ok1 = false, ok2 = false;
    EXPECT_EQ(VirtualFs::readFile(h1, "/home/user/file.txt", ok1), "from-a");
    EXPECT_EQ(VirtualFs::readFile(h2, "/home/user/file.txt", ok2), "from-b");
    EXPECT_TRUE(ok1);
    EXPECT_TRUE(ok2);

    VirtualFs::clearChanges();
}

TEST(vfs_generated_file) {
    // Системный файл возвращает динамически генерируемый текст.
    Host h = makeVfsHost();
    VirtualFs::clearChanges();

    int counter = 0;
    bool added = VirtualFs::addGeneratedFile(h, "/home/user/gen.txt",
                                              [&counter](Session*) {
                                                  return "gen-" + std::to_string(++counter);
                                              });
    EXPECT_TRUE(added);
    EXPECT_TRUE(VirtualFs::isSystemFile(h, "/home/user/gen.txt"));

    bool ok = false;
    EXPECT_EQ(VirtualFs::readFile(h, "/home/user/gen.txt", ok), "gen-1");
    EXPECT_TRUE(ok);
    EXPECT_EQ(VirtualFs::readFile(h, "/home/user/gen.txt", ok), "gen-2");

    VirtualFs::clearChanges();
}
