#pragma once

class Session;

// Полноэкранный двухпанельный файловый менеджер.
// Вся работа с FTXUI спрятана в .cpp; наружу — только обычные типы C++.
void runFileManager(Session& session);
