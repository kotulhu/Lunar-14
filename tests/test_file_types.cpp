#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include "assert.h"
#include "env.h"
#include "registry.h"

#include "commands/CatCommand.h"
#include "core/Host.h"
#include "core/SaveGame.h"
#include "core/Session.h"
#include "core/VirtualFs.h"
#include "core/World.h"
#include "ui/Terminal.h"

TEST(file_type_from_json_and_default) {
    World world;
    std::string error;
    if (TestEnv::data_dir.empty() || !world.load(TestEnv::data_dir, error)) {
        testing::recordFailure("cannot load world: " + error);
        return;
    }
    const Host* home = world.findHost("home-pc");
    ASSERT_EQ(home != nullptr, true);
    EXPECT_TRUE(VirtualFs::typeOf(*home, "/home/hacker/.diagnostic") == FileType::Program);
    EXPECT_TRUE(VirtualFs::typeOf(*home, "/home/hacker/.note.txt") == FileType::Data);
    EXPECT_TRUE(VirtualFs::typeOf(*home, "/home/hacker/readme.txt") == FileType::Text);
}

TEST(downloads_path_is_reserved_in_virtual_fs) {
    World world;
    std::string error;
    if (TestEnv::data_dir.empty() || !world.load(TestEnv::data_dir, error)) {
        testing::recordFailure("cannot load world: " + error);
        return;
    }
    const Host* home = world.findHost("home-pc");
    ASSERT_EQ(home != nullptr, true);
    EXPECT_TRUE(VirtualFs::isReservedPath("/home/hacker/downloads/item"));
    EXPECT_FALSE(VirtualFs::exists(*home, "/home/hacker/downloads"));
    EXPECT_FALSE(VirtualFs::writeFile(*home, "/home/hacker/downloads/item", "x"));
}

TEST(local_storage_list_and_json_roundtrip) {
    LocalStorage storage;
    storage.add({"zeta.txt", FileType::Text, "z", "host-b", "/zeta.txt"});
    storage.add({"alpha.bin", FileType::Program, "bin", "host-a", "/alpha.bin"});
    EXPECT_TRUE(storage.has("alpha.bin"));
    EXPECT_TRUE(storage.get("missing") == nullptr);
    EXPECT_EQ(storage.list().size(), static_cast<std::size_t>(2));
    EXPECT_EQ(storage.list()[0].name, "alpha.bin");

    const std::string json = storage.toJson();
    EXPECT_TRUE(json.find("\"type\":\"program\"") != std::string::npos);
    LocalStorage restored;
    std::string error;
    EXPECT_TRUE(restored.fromJson(json, error));
    const DownloadedFile* file = restored.get("alpha.bin");
    EXPECT_TRUE(file != nullptr);
    if (file) {
        EXPECT_TRUE(file->type == FileType::Program);
        EXPECT_EQ(file->content, "bin");
        EXPECT_EQ(file->sourceHost, "host-a");
        EXPECT_EQ(file->sourcePath, "/alpha.bin");
    }
}

TEST(savegame_downloads_roundtrip) {
    World world;
    std::string error;
    if (TestEnv::data_dir.empty() || !world.load(TestEnv::data_dir, error)) {
        testing::recordFailure("cannot load world: " + error);
        return;
    }
    Session before(world, "home-pc");
    before.localStorage().add(
        {"clue.dat", FileType::Data, "coordinates", "relay-hub", "/var/clue.dat"});
    const std::string save = SaveGame::serialize(before);

    Session after(world, "home-pc");
    std::string warning;
    EXPECT_TRUE(SaveGame::deserialize(save, after, world, error, warning));
    const DownloadedFile* restored = after.localStorage().get("clue.dat");
    EXPECT_TRUE(restored != nullptr);
    if (restored) {
        EXPECT_TRUE(restored->type == FileType::Data);
        EXPECT_EQ(restored->content, "coordinates");
        EXPECT_EQ(restored->sourceHost, "relay-hub");
        EXPECT_EQ(restored->sourcePath, "/var/clue.dat");
    }
}

TEST(cat_rejects_program_and_reads_data) {
    World world;
    std::string error;
    if (TestEnv::data_dir.empty() || !world.load(TestEnv::data_dir, error)) {
        testing::recordFailure("cannot load world: " + error);
        return;
    }
    Session session(world, "home-pc");
    CatCommand cat;
    Terminal term;
    std::ostringstream captured;
    std::streambuf* previous = std::cout.rdbuf(captured.rdbuf());

    session.localStorage().add(
        {"diagnostic", FileType::Program, "binary", "relay-hub", "/bin/diagnostic"});
    session.localStorage().add(
        {"report.txt", FileType::Text, "local report", "relay-hub", "/tmp/report.txt"});
    cat.execute(session, world, {"downloads/diagnostic"}, term);
    std::string program_output = captured.str();
    EXPECT_TRUE(program_output.find("cat: downloads/diagnostic: binary file, use 'run'") !=
                std::string::npos);

    captured.str("");
    captured.clear();
    cat.execute(session, world, {"downloads/report.txt"}, term);
    std::string text_output = captured.str();
    captured.str("");
    captured.clear();
    cat.execute(session, world, {"downloads/missing"}, term);
    std::cout.rdbuf(previous);
    EXPECT_EQ(text_output, "local report\n");
    EXPECT_EQ(captured.str(), "cat: downloads/missing: no such file\n");
}
