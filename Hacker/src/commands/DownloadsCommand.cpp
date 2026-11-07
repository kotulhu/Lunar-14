#include "commands/DownloadsCommand.h"

#include "core/LocalStorage.h"
#include "core/Session.h"
#include "ui/Terminal.h"

void DownloadsCommand::execute(Session& session, const World&,
                               const std::vector<std::string>&, Terminal& term) {
    term.print("NAME                         TYPE        SIZE  SOURCE");
    for (const DownloadedFile& file : session.localStorage().list()) {
        term.print(file.name + std::string(file.name.size() < 28 ? 28 - file.name.size() : 1, ' ') +
                   fileTypeName(file.type) + std::string(12 - std::string(fileTypeName(file.type)).size(), ' ') +
                   std::to_string(file.content.size()) + "  " +
                   file.sourceHost + ":" + file.sourcePath);
    }
}
