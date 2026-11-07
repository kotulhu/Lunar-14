#include <iostream>
#include <sstream>
#include <string>

#include "assert.h"
#include "env.h"
#include "registry.h"

#include "commands/BroadcastCommand.h"
#include "commands/RunCommand.h"
#include "core/Host.h"
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

std::string executeRun(Session& session, const World& world,
                       const std::vector<std::string>& args, Terminal& term) {
    RunCommand command;
    std::ostringstream output;
    std::streambuf* previous = std::cout.rdbuf(output.rdbuf());
    command.execute(session, world, args, term);
    std::cout.rdbuf(previous);
    return output.str();
}

std::string executeBroadcast(Session& session, const World& world, Terminal& term) {
    BroadcastCommand command;
    std::ostringstream output;
    std::streambuf* previous = std::cout.rdbuf(output.rdbuf());
    command.execute(session, world, {}, term);
    std::cout.rdbuf(previous);
    return output.str();
}

void addMailsend(Session& session) {
    session.localStorage().add(
        {"mailsend", FileType::Program, "JSC Mail Dispatch Utility. Requires a data file.",
         "ftp.jsc-files", "/pub/mailsend", "mailsend"});
    session.localStorage().add(
        {"leak.txt", FileType::Data, "secret", "ftp.jsc-files", "/pub/leak.txt"});
}

}  // namespace

TEST(run_mailsend_dispatches_locally) {
    // Локальный запуск показывает получателей из data/mailsend.json и финал.
    World world;
    std::string error;
    if (!loadTestWorld(world, error)) {
        testing::recordFailure("cannot load world: " + error);
        return;
    }

    Session session(world, "home-pc");
    addMailsend(session);
    Terminal term;
    term.setNoDelay(true);

    const std::string output = executeRun(session, world, {"mailsend", "leak.txt"}, term);
    EXPECT_TRUE(output.find("Mode: local dispatch") != std::string::npos);
    EXPECT_TRUE(output.find("СПИСОК ПОЛУЧАТЕЛЕЙ") != std::string::npos);
    EXPECT_TRUE(output.find("Новостные каналы") != std::string::npos);
    EXPECT_TRUE(output.find("Институты") != std::string::npos);
    EXPECT_TRUE(output.find("Разработчик") != std::string::npos);
    EXPECT_TRUE(output.find("Спонсоры") != std::string::npos);
    EXPECT_TRUE(output.find("Сюда можно добавить свой текст") != std::string::npos);
    EXPECT_TRUE(output.find("THE END") != std::string::npos);
    EXPECT_TRUE(session.gameState().finished());
    EXPECT_FALSE(session.gameState().payloadReady());
}

TEST(run_mailsend_requires_data_file) {
    World world;
    std::string error;
    if (!loadTestWorld(world, error)) {
        testing::recordFailure("cannot load world: " + error);
        return;
    }

    Session session(world, "relay-hub");
    session.localStorage().add(
        {"mailsend", FileType::Program, "JSC Mail Dispatch Utility. Requires a data file.",
         "ftp.jsc-files", "/pub/mailsend", "mailsend"});
    Terminal term;
    term.setNoDelay(true);

    const std::string output = executeRun(session, world, {"mailsend"}, term);
    EXPECT_TRUE(output.find("mailsend: missing data file") != std::string::npos);
    EXPECT_FALSE(session.gameState().payloadReady());
}

TEST(run_mailsend_rejects_non_data_file) {
    World world;
    std::string error;
    if (!loadTestWorld(world, error)) {
        testing::recordFailure("cannot load world: " + error);
        return;
    }

    Session session(world, "relay-hub");
    session.localStorage().add(
        {"mailsend", FileType::Program, "JSC Mail Dispatch Utility. Requires a data file.",
         "ftp.jsc-files", "/pub/mailsend", "mailsend"});
    session.localStorage().add(
        {"notes.txt", FileType::Text, "not a payload", "ftp.jsc-files", "/pub/notes.txt"});
    Terminal term;
    term.setNoDelay(true);

    const std::string output = executeRun(session, world, {"mailsend", "notes.txt"}, term);
    EXPECT_TRUE(output.find("mailsend: notes.txt: expected a data file") != std::string::npos);
    EXPECT_FALSE(session.gameState().payloadReady());
}

TEST(run_mailsend_stages_payload_on_relay) {
    World world;
    std::string error;
    if (!loadTestWorld(world, error)) {
        testing::recordFailure("cannot load world: " + error);
        return;
    }

    Session session(world, "relay-hub");
    addMailsend(session);
    Terminal term;
    term.setNoDelay(true);

    const std::string output = executeRun(session, world, {"mailsend", "leak.txt"}, term);
    EXPECT_TRUE(output.find("Payload staged") != std::string::npos);
    EXPECT_TRUE(session.gameState().payloadReady());
}

TEST(run_diagnostic_program) {
    World world;
    std::string error;
    if (!loadTestWorld(world, error)) {
        testing::recordFailure("cannot load world: " + error);
        return;
    }

    Session session(world, "home-pc");
    Terminal term;
    term.setNoDelay(true);

    const std::string output = executeRun(session, world, {".diagnostic"}, term);
    EXPECT_TRUE(output.find("diagnostic utility") != std::string::npos);
}

TEST(broadcast_requires_staged_payload) {
    World world;
    std::string error;
    if (!loadTestWorld(world, error)) {
        testing::recordFailure("cannot load world: " + error);
        return;
    }

    Session session(world, "relay-hub");
    Terminal term;
    term.setNoDelay(true);

    const std::string blocked = executeBroadcast(session, world, term);
    EXPECT_TRUE(blocked.find("broadcast: no payload staged") != std::string::npos);
    EXPECT_FALSE(session.gameState().finished());

    session.gameState().setPayloadReady(true);
    const std::string output = executeBroadcast(session, world, term);
    EXPECT_TRUE(output.find("Broadcast started.") != std::string::npos);
    EXPECT_TRUE(session.gameState().finished());
}
