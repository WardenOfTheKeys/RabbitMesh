// RabbitMesh MECP (Deck > MECP): write Mesh Emergency Communication Protocol messages
// and read the ones in your chats. The format and code table are in mecp_core.h.
//
//   Write:  severity, codes picked from twelve categories, how many people, your GPS
//           position, a short note, the channel; the exact text is shown before it goes,
//           and a MAYDAY asks once more.
//   Inbox:  every MECP line in your chat history, decoded, newest first, coloured by
//           severity; one with a position opens on the map.
//   Alerts: a MAYDAY or URGENT arriving on a channel raises a banner and a buzz even
//           when that channel is muted (Settings in the MECP menu turns the override off).
#pragma once
#include <Arduino.h>

struct HistMsg;

namespace mecp {
  void open();                                     // the write menu
  void openInbox();
  // main.cpp, for every channel message as it arrives. True when it was a MAYDAY or
  // URGENT that this raised its own alert for (the usual message alert is then skipped).
  bool onChannelMessage(const char* channelName, const HistMsg& m);
  uint8_t urgentWaiting();                         // MAYDAY/URGENT heard since the inbox was last opened
  void tick();                                     // deck::tick(): the siren's volume back to the user's after it
}
