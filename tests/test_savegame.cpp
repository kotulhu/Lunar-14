// Тесты SaveGame: round-trip, отсутствующие поля, version mismatch и т.п.
#include "assert.h"
#include "env.h"
#include "registry.h"

#include "core/SaveGame.h"
#include "core/Session.h"
#include "core/VirtualFs.h"
#include "core/World.h"

// Загружает мир из TestEnv::data_dir.
// При отсутствии данных записывает ошибку и возвращает false.
static bool loadWorld(World& world) {
    if (TestEnv::data_dir.empty()) {
        testing::recordFailure("data/ не найдена — тест требует папку data/");
        return false;
    }
    std::string err;
    if (!world.load(TestEnv::data_dir, err)) {
        testing::recordFailure("World::load failed: " + err);
        return false;
    }
    return true;
}

// ── Round-trip ───────────────────────────────────────────────────────────────

TEST(savegame_roundtrip_empty_session) {
    // Минимальный round-trip: пустая сессия → serialize → deserialize → re-serialize.
    // Второй serialize должен совпасть с первым.
    World world;
    if (!loadWorld(world)) return;
    VirtualFs::clearChanges();

    Session s1(world, world.playerHost());
    std::string first = SaveGame::serialize(s1);
    EXPECT_FALSE(first.empty());

    Session s2(world, world.playerHost());
    std::string err, warn;
    bool ok = SaveGame::deserialize(first, s2, world, err, warn);
    EXPECT_TRUE(ok);
    if (!ok) {
        testing::recordFailure("deserialize error: " + err);
        VirtualFs::clearChanges();
        return;
    }

    std::string second = SaveGame::serialize(s2);
    EXPECT_EQ(first, second);

    VirtualFs::clearChanges();
}

TEST(savegame_finished_flag) {
    // Флаг finished сохраняется и восстанавливается.
    World world;
    if (!loadWorld(world)) return;
    VirtualFs::clearChanges();

    Session s1(world, world.playerHost());
    s1.gameState().setFinished(true);

    std::string json = SaveGame::serialize(s1);

    Session s2(world, world.playerHost());
    std::string err, warn;
    SaveGame::deserialize(json, s2, world, err, warn);

    EXPECT_TRUE(s2.gameState().finished());
    VirtualFs::clearChanges();
}

TEST(savegame_payload_ready_flag) {
    // Флаг загруженного на ретранслятор payload сохраняется и восстанавливается.
    World world;
    if (!loadWorld(world)) return;
    VirtualFs::clearChanges();

    Session s1(world, world.playerHost());
    s1.gameState().setPayloadReady(true);

    std::string json = SaveGame::serialize(s1);

    Session s2(world, world.playerHost());
    std::string err, warn;
    SaveGame::deserialize(json, s2, world, err, warn);

    EXPECT_TRUE(s2.gameState().payloadReady());
    VirtualFs::clearChanges();
}

TEST(savegame_visited_entry) {
    // Запись о посещённом узле сохраняется и восстанавливается.
    World world;
    if (!loadWorld(world)) return;
    VirtualFs::clearChanges();

    Session s1(world, world.playerHost());
    // Домашний хост гарантированно существует в world.
    const std::string home = world.playerHost();
    s1.gameState().record(home, 23, "hacker", "pass123");

    std::string json = SaveGame::serialize(s1);

    Session s2(world, world.playerHost());
    std::string err, warn;
    bool ok = SaveGame::deserialize(json, s2, world, err, warn);
    EXPECT_TRUE(ok);

    const VisitedEntry* e = s2.gameState().find(home, 23, "hacker");
    EXPECT_TRUE(e != nullptr);
    if (e) {
        EXPECT_EQ(e->pass, "pass123");
    }

    VirtualFs::clearChanges();
}

TEST(savegame_fired_events) {
    // Список сработавших событий сохраняется и восстанавливается.
    World world;
    if (!loadWorld(world)) return;
    VirtualFs::clearChanges();

    Session s1(world, world.playerHost());
    s1.gameState().markEventFired("newsletter");
    s1.gameState().markEventFired("bribe");

    std::string json = SaveGame::serialize(s1);

    Session s2(world, world.playerHost());
    std::string err, warn;
    SaveGame::deserialize(json, s2, world, err, warn);

    EXPECT_TRUE(s2.gameState().eventFired("newsletter"));
    EXPECT_TRUE(s2.gameState().eventFired("bribe"));
    EXPECT_FALSE(s2.gameState().eventFired("unknown-event"));

    VirtualFs::clearChanges();
}

// ── Ошибочный/неполный JSON ──────────────────────────────────────────────────

TEST(savegame_version_mismatch) {
    // Несовместимая версия → deserialize возвращает false с ошибкой.
    World world;
    if (!loadWorld(world)) return;

    std::string json =
        R"({"version": 999, "state": {"visited": [], "events": [], "finished": false}})";

    Session s(world, world.playerHost());
    std::string err, warn;
    bool ok = SaveGame::deserialize(json, s, world, err, warn);
    EXPECT_FALSE(ok);
    EXPECT_FALSE(err.empty());
}

TEST(savegame_missing_state) {
    // Нет поля "state" → deserialize возвращает false.
    World world;
    if (!loadWorld(world)) return;

    std::string json = R"({"version": 1})";

    Session s(world, world.playerHost());
    std::string err, warn;
    bool ok = SaveGame::deserialize(json, s, world, err, warn);
    EXPECT_FALSE(ok);
    EXPECT_FALSE(err.empty());
}

TEST(savegame_minimal_state) {
    // Пустой state без опциональных разделов → успех.
    World world;
    if (!loadWorld(world)) return;

    std::string json = R"({"version": 1, "state": {}})";

    Session s(world, world.playerHost());
    std::string err, warn;
    bool ok = SaveGame::deserialize(json, s, world, err, warn);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(err.empty());
}

TEST(savegame_invalid_json) {
    // Битый JSON → deserialize возвращает false с ошибкой.
    World world;
    if (!loadWorld(world)) return;

    std::string bad = "this is not json {{{";

    Session s(world, world.playerHost());
    std::string err, warn;
    bool ok = SaveGame::deserialize(bad, s, world, err, warn);
    EXPECT_FALSE(ok);
    EXPECT_FALSE(err.empty());
}

TEST(savegame_unknown_host_skipped) {
    // Неизвестный хост в visited → пропускается с предупреждением, но не ошибкой.
    World world;
    if (!loadWorld(world)) return;

    std::string json = R"({
        "version": 1,
        "state": {
            "visited": [
                {"host": "no-such-host-xyz", "port": 22, "user": "x", "pass": "y"}
            ]
        }
    })";

    Session s(world, world.playerHost());
    std::string err, warn;
    bool ok = SaveGame::deserialize(json, s, world, err, warn);
    EXPECT_TRUE(ok);           // ошибки нет — просто предупреждение
    EXPECT_FALSE(warn.empty()); // предупреждение о пропуске
    EXPECT_TRUE(err.empty());
}
