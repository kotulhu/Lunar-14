#include <chrono>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "assert.h"
#include "env.h"
#include "registry.h"

#include "commands/CatCommand.h"
#include "commands/CdCommand.h"
#include "commands/LsCommand.h"
#include "commands/TelnetCommand.h"
#include "core/Session.h"
#include "core/VirtualFs.h"
#include "core/World.h"
#include "ui/Terminal.h"

namespace {

bool loadWorld(World& world, std::string& error) {
    if (TestEnv::data_dir.empty()) {
        error = "data directory not found";
        return false;
    }
    return world.load(TestEnv::data_dir, error);
}

std::string catFile(Session& session, const World& world, const std::string& path) {
    CatCommand cat;
    Terminal term;
    std::ostringstream captured;
    std::streambuf* previous = std::cout.rdbuf(captured.rdbuf());
    cat.execute(session, world, {path}, term);
    std::cout.rdbuf(previous);
    return captured.str();
}

}  // namespace

TEST(generated_auth_log_changes_after_telnet) {
    World world;
    std::string error;
    if (!loadWorld(world, error)) {
        testing::recordFailure("cannot load world: " + error);
        return;
    }

    Session session(world, "home-pc");
    bool ok = false;
    const std::string before =
        VirtualFs::readFile(session.host(), "/var/log/auth.log", ok, &session);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(VirtualFs::isSystemFile(session.host(), "/var/log/auth.log"));

    Terminal term;
    term.setNoDelay(true);
    TelnetCommand telnet;
    std::ostringstream captured;
    std::streambuf* previous = std::cout.rdbuf(captured.rdbuf());
    telnet.execute(session, world, {"dead-host"}, term);
    session.gameState().record("old-mailserver", 23, "guest", "guest");
    telnet.execute(session, world, {"old-mailserver"}, term);
    std::cout.rdbuf(previous);

    while (session.depth() > 1) session.pop();
    const std::string after =
        VirtualFs::readFile(session.host(), "/var/log/auth.log", ok, &session);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(after != before);
    EXPECT_TRUE(after.find("dead-host") != std::string::npos);
    EXPECT_TRUE(after.find("failed") != std::string::npos);
    EXPECT_TRUE(after.find("old-mailserver") != std::string::npos);
    EXPECT_TRUE(after.find("accepted") != std::string::npos);
}

TEST(generated_proc_uptime_grows) {
    World world;
    std::string error;
    if (!loadWorld(world, error)) {
        testing::recordFailure("cannot load world: " + error);
        return;
    }

    Session session(world, "relay-hub");
    bool ok = false;
    const std::string first =
        VirtualFs::readFile(session.host(), "/proc/uptime", ok, &session);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(VirtualFs::isSystemFile(session.host(), "/proc/uptime"));

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    const std::string second =
        VirtualFs::readFile(session.host(), "/proc/uptime", ok, &session);
    EXPECT_TRUE(ok);

    const double a = std::stod(first);
    const double b = std::stod(second);
    EXPECT_TRUE(b > a);
}

TEST(generated_ufo_motd_from_pool) {
    World world;
    std::string error;
    if (!loadWorld(world, error)) {
        testing::recordFailure("cannot load world: " + error);
        return;
    }

    const Host* bbs = world.findHost("ufo-bbs");
    ASSERT_EQ(bbs != nullptr, true);
    EXPECT_TRUE(VirtualFs::isSystemFile(*bbs, "/etc/motd"));

    const std::vector<std::string> pool = {
        "The truth is out there. The disk space is not.",
        "I want to believe. I also want a faster modem.",
        "Trust no one. Especially not root.",
        "This BBS has been expecting you.",
        "We are not alone. Check /posts.",
        "Houston, we have a login.",
    };

    bool ok = false;
    const std::string motd = VirtualFs::readFile(*bbs, "/etc/motd", ok);
    EXPECT_TRUE(ok);
    bool known = false;
    for (const std::string& quote : pool) {
        if (motd == quote) known = true;
    }
    EXPECT_TRUE(known);

    const Host* ftp = world.findHost("ftp.jsc-files");
    ASSERT_EQ(ftp != nullptr, true);
    const std::string ftp_motd = VirtualFs::readFile(*ftp, "/etc/motd", ok);
    EXPECT_TRUE(ok);
    EXPECT_EQ(ftp_motd, "Welcome to JSC-FTP, since 1994");
}

TEST(local_downloads_folder_is_visible) {
    World world;
    std::string error;
    if (!loadWorld(world, error)) {
        testing::recordFailure("cannot load world: " + error);
        return;
    }

    Session session(world, "home-pc");
    EXPECT_EQ(session.localDownloadsDir(), "/home/hacker/downloads");
    EXPECT_TRUE(session.isLocalDownloadsDir("/home/hacker/downloads"));
    EXPECT_TRUE(session.isLocalDownloadsParent("/home/hacker"));
    EXPECT_FALSE(session.isLocalDownloadsDir("/home/hacker"));

    session.localStorage().add(
        {"mail.txt", FileType::Text, "hello", "old-mailserver", "/home/guest/mail.txt"});
    EXPECT_EQ(session.listLocalDownloads(false).size(), static_cast<std::size_t>(1));

    Terminal term;
    term.setNoDelay(true);
    CdCommand cd;
    cd.execute(session, world, {"downloads"}, term);
    EXPECT_EQ(session.currentDir(), "/home/hacker/downloads");

    LsCommand ls;
    std::ostringstream captured;
    std::streambuf* previous = std::cout.rdbuf(captured.rdbuf());
    ls.execute(session, world, {}, term);
    std::cout.rdbuf(previous);
    EXPECT_TRUE(captured.str().find("mail.txt") != std::string::npos);
}

TEST(basic_commands_allowed_on_restricted_host) {
    World world;
    std::string error;
    if (!loadWorld(world, error)) {
        testing::recordFailure("cannot load world: " + error);
        return;
    }

    const Host* mail = world.findHost("old-mailserver");
    ASSERT_EQ(mail != nullptr, true);
    EXPECT_TRUE(mail->allows("ls"));
    EXPECT_TRUE(mail->allows("cat"));
    EXPECT_TRUE(mail->allows("cd"));
    EXPECT_TRUE(mail->allows("pwd"));
    EXPECT_TRUE(mail->allows("run"));
    EXPECT_FALSE(mail->allows("telnet"));
    EXPECT_FALSE(mail->allows("mail"));
}

TEST(generated_bash_history_and_dead_mail) {
    World world;
    std::string error;
    if (!loadWorld(world, error)) {
        testing::recordFailure("cannot load world: " + error);
        return;
    }

    Session home(world, "home-pc");
    bool ok = false;
    const std::string history =
        VirtualFs::readFile(home.host(), "/home/hacker/.bash_history", ok, &home);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(history.find("sudo rm -rf /") != std::string::npos);
    EXPECT_TRUE(history.find("# не смейся, я один раз нажал") != std::string::npos);

    Session dead(world, "dead-host");
    EXPECT_FALSE(VirtualFs::exists(dead.host(), "/var/spool/mail/root"));
    const std::string output = catFile(dead, world, "/var/spool/mail/root");
    EXPECT_TRUE(output.find("No such file or directory") != std::string::npos);
    EXPECT_TRUE(output.find("555-0147") != std::string::npos);
}
