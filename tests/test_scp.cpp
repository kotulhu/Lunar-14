#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include "assert.h"
#include "env.h"
#include "registry.h"

#include "commands/ScpCommand.h"
#include "core/EventEngine.h"
#include "core/Session.h"
#include "core/World.h"
#include "ui/Terminal.h"

namespace {

bool loadTestWorld(World& world, std::string& error) {
    if (TestEnv::data_dir.empty()) {
        error = "data directory not found";
        return false;
    }
    return world.load(TestEnv::data_dir, error);
}

std::string executeScp(Session& session, const World& world,
                       const std::vector<std::string>& args) {
    ScpCommand command;
    Terminal term;
    std::ostringstream output;
    std::streambuf* previous = std::cout.rdbuf(output.rdbuf());
    command.execute(session, world, args, term);
    std::cout.rdbuf(previous);
    return output.str();
}

}  // namespace

TEST(scp_cwd_and_absolute_paths_preserve_type) {
    World world;
    std::string error;
    if (!loadTestWorld(world, error)) {
        testing::recordFailure("cannot load world: " + error);
        return;
    }
    Session session(world, "home-pc");
    const std::string cwd_output = executeScp(session, world, {".diagnostic"});
    const DownloadedFile* file = session.localStorage().get(".diagnostic");
    EXPECT_TRUE(file != nullptr);
    EXPECT_TRUE(cwd_output.find("scp: .diagnostic (") == 0);
    EXPECT_TRUE(cwd_output.find(" -> downloads/.diagnostic\n") != std::string::npos);
    if (file) EXPECT_TRUE(file->type == FileType::Program);

    const std::string absolute_output =
        executeScp(session, world, {"/home/hacker/.note.txt"});
    const DownloadedFile* data = session.localStorage().get(".note.txt");
    EXPECT_TRUE(data != nullptr);
    EXPECT_TRUE(absolute_output.find(" -> downloads/.note.txt\n") != std::string::npos);
    if (data) EXPECT_TRUE(data->type == FileType::Data);
}

TEST(scp_missing_file_message) {
    World world;
    std::string error;
    if (!loadTestWorld(world, error)) {
        testing::recordFailure("cannot load world: " + error);
        return;
    }
    Session session(world, "home-pc");
    EXPECT_EQ(executeScp(session, world, {"missing.bin"}),
              "scp: missing.bin: no such file\n");
    EXPECT_FALSE(session.localStorage().has("missing.bin"));
}

TEST(scp_overwrites_and_emits_download_signal) {
    World world;
    std::string error;
    if (!loadTestWorld(world, error)) {
        testing::recordFailure("cannot load world: " + error);
        return;
    }
    Session session(world, "home-pc");
    session.localStorage().add(
        {".diagnostic", FileType::Text, "old", "old-host", "/old"});
    std::string chat_error;
    if (!session.chat().loadFromFile(TestEnv::data_dir + "/chat.json", chat_error)) {
        testing::recordFailure("cannot load chat.json: " + chat_error);
        return;
    }
    auto engine = std::make_unique<EventEngine>();
    if (!engine->load(TestEnv::data_dir, world, session.chat(), error) ||
        !engine->loadRulesJson(
            R"([{"id":"downloaded", "conditions":{"downloaded":[".diagnostic"]}, "actions":[{"unlock_contact":"diona"}]}])",
            world, session.chat(), error)) {
        testing::recordFailure("cannot load event rules: " + error);
        return;
    }
    session.setEventEngine(std::move(engine));

    const std::string output = executeScp(session, world, {".diagnostic"});
    const DownloadedFile* file = session.localStorage().get(".diagnostic");
    EXPECT_TRUE(output.find("scp: overwriting downloads/.diagnostic\n") == 0);
    EXPECT_TRUE(session.gameState().eventFired("downloaded"));
    EXPECT_TRUE(session.chat().find("diona")->visible);
    EXPECT_TRUE(file != nullptr);
    if (file) EXPECT_TRUE(file->type == FileType::Program);
}
