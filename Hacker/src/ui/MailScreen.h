#pragma once

class Session;

// Полноэкранный почтовый клиент.
// Вся работа с FTXUI спрятана в .cpp; наружу — только обычные типы C++.
void runMail(Session& session);
