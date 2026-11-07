#include <memory>
#include <string>

#include "assert.h"
#include "env.h"
#include "registry.h"

#include "core/EventEngine.h"
#include "core/Session.h"
#include "core/World.h"

TEST(event_and_conditions_across_signals) {
    World world;
    std::string error;
    if (TestEnv::data_dir.empty() || !world.load(TestEnv::data_dir, error)) {
        testing::recordFailure("cannot load world: " + error);
        return;
    }
    Session session(world, world.playerHost());
    if (!session.chat().loadFromFile(TestEnv::data_dir + "/chat.json", error)) {
        testing::recordFailure("cannot load chat.json: " + error);
        return;
    }
    auto engine = std::make_unique<EventEngine>();
    if (!engine->load(TestEnv::data_dir, world, session.chat(), error)) {
        testing::recordFailure("cannot load events: " + error);
        return;
    }
    const std::string rules = R"([
      {"id":"all-signals", "conditions":{
        "visited":["old-mailserver"], "visited_count":1,
        "downloaded":["relay_keys.dat"], "file_read":["/notes/hint.txt"],
        "mail_read":["ghost_01"], "command":["portscan"]
      }, "actions":[{"unlock_contact":"diona"}]}
    ])";
    if (!engine->loadRulesJson(rules, world, session.chat(), error)) {
        testing::recordFailure("cannot load test event: " + error);
        return;
    }
    session.setEventEngine(std::move(engine));

    EventEngine* events = session.events();
    events->onLogin("old-mailserver");
    EXPECT_FALSE(session.gameState().eventFired("all-signals"));
    session.gameState().record("old-mailserver", 23, "guest", "guest");
    events->onLogin("old-mailserver");
    events->onDownload("relay_keys.dat");
    events->onFileRead("old-mailserver", "/notes/hint.txt");
    events->onMailRead("ghost_01");
    EXPECT_FALSE(session.gameState().eventFired("all-signals"));
    events->onCommand("portscan");
    EXPECT_TRUE(session.gameState().eventFired("all-signals"));
    EXPECT_TRUE(session.chat().find("diona")->visible);
}

TEST(event_once_deduplication) {
    World world;
    std::string error;
    if (TestEnv::data_dir.empty() || !world.load(TestEnv::data_dir, error)) {
        testing::recordFailure("cannot load world: " + error);
        return;
    }
    Session session(world, world.playerHost());
    if (!session.chat().loadFromFile(TestEnv::data_dir + "/chat.json", error)) {
        testing::recordFailure("cannot load chat.json: " + error);
        return;
    }
    auto engine = std::make_unique<EventEngine>();
    if (!engine->load(TestEnv::data_dir, world, session.chat(), error)) {
        testing::recordFailure("cannot load events: " + error);
        return;
    }
    const std::string rules = R"([
      {"id":"once-only", "conditions":{"visited":["jsc-archive"]},
       "actions":[{"chat":{"contact":"wolf", "text":"once"}}]}
    ])";
    if (!engine->loadRulesJson(rules, world, session.chat(), error)) {
        testing::recordFailure("cannot load test event: " + error);
        return;
    }
    session.setEventEngine(std::move(engine));
    session.gameState().record("jsc-archive", 2222, "admin", "pw");
    const std::size_t before = session.chat().history("wolf").size();
    session.events()->onLogin("jsc-archive");
    session.events()->onCommand("help");
    session.events()->onLogin("jsc-archive");
    EXPECT_EQ(session.chat().history("wolf").size(), before + 1);
    EXPECT_TRUE(session.gameState().eventFired("once-only"));
}
