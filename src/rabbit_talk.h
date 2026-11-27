// What the Neon Rabbit says (RabbitMesh). Same kinds, same 28-character bubble and the
// same no-repeat decks as the sasquatch's lines in squatch_talk.h, which picks these
// when the theme's scene is SCENE_NEONRABBIT. Included by squatch_talk.h inside
// namespace talk.

static const char* const R_SHAKE_L[] = {
  "Whoa! Signal jitter!", "Easy on the chassis.", "My ears just rebooted.", "Turbulence in the grid.",
  "Did we hit a speed bump?", "Gyro says: rude.", "Careful, I'm wetware.", "That scrambled my cache.",
  "Hop first, shake later.", "Packets everywhere now.", "Stabilizers offline!", "Hey, I was mid-hop!",
  "Earthquake or hacker?", "Rain's bad enough.", "Shaken, not encrypted."};
static const char* const R_HARD_L[] = {
  "SYSTEM SHOCK!", "Put me DOWN, choom!", "Ears! Mind the ears!", "Kernel panic! Panic!",
  "Too many g's!", "Blender mode? Really?", "My visor's cracked!", "AAAH! Abort! Abort!"};
static const char* const R_AGAIN_L[] = {
  "Okay, I'm awake! Awake!", "Fine. Fully booted.", "You're a menace.", "Again? I'll file a bug.",
  "Seeing double rabbits."};
static const char* const R_MORN_L[] = {
  "Morning. Neon's still on.", "Coffee. Then carrots.", "Grid's quiet. For now.", "Rise and route.",
  "Up early? Me too.", "Sun's up. Visor down.", "Morning sync complete.", "New day, new hops."};
static const char* const R_DAY_L[] = {
  "Hey choom!", "Mesh is humming today.", "Want to go wardriving?", "Repeaters say hi.",
  "Nice weather for packets.", "Ready when you are.", "Scanning the streets.", "Hop to it!"};
static const char* const R_EVE_L[] = {
  "City lights are on.", "Evening, runner.", "Neon hours begin.", "Good night for a drive.",
  "Rain's rolling in.", "Signs are flickering on.", "Prime time for packets."};
static const char* const R_LATE_L[] = {
  "Night owl, huh?", "Still jacked in?", "The grid never sleeps.", "Only us and repeaters.",
  "Go to bed, choom.", "Late shift again?", "*yawn* ...low power."};
static const char* const R_MSG_L[] = {
  "Incoming transmission!", "Somebody pinged you!", "New message, choom!", "Packet with your name!",
  "Mail's in!", "Ears up: a message!"};
static const char* const R_PLUG_L[] = {
  "Power! Sweet power.", "Charging. Mmm, volts.", "Jacked in. Thanks!", "Ears tingling. Good.",
  "Fueling up.", "Battery go brrr."};
static const char* const R_LOW_L[] = {
  "Running on fumes...", "Need a charge, choom.", "Battery's crashing.", "Visor dimming...",
  "Plug me in? Please?", "Low power mode..."};
static const char* const R_LEAN_L[] = {
  "Whoa, tilt warning.", "Gravity's glitching.", "Hold me level, choom.", "I'm sliding!"};
static const char* const R_POKE_L[] = {
  "Boop received.", "Hey! That's my nose.", "Ping! Pong!", "Touch input logged.",
  "Hi there!", "You rang?", "Ears up!", "Easy, that's a visor."};
static const char* const R_POKE2_L[] = {
  "Stop poking the rabbit!", "Rate limited. Back off.", "Okay, that's spam.", "Too many boops!"};

#define TALK_SET(a) {a, (uint8_t)(sizeof(a) / sizeof(a[0]))}
static const Set RABBIT_SETS[KIND_COUNT] = {
  TALK_SET(R_SHAKE_L), TALK_SET(R_HARD_L), TALK_SET(R_AGAIN_L), TALK_SET(R_MORN_L), TALK_SET(R_DAY_L),
  TALK_SET(R_EVE_L), TALK_SET(R_LATE_L), TALK_SET(R_MSG_L), TALK_SET(R_PLUG_L), TALK_SET(R_LOW_L), TALK_SET(R_LEAN_L),
  TALK_SET(R_POKE_L), TALK_SET(R_POKE2_L)};
#undef TALK_SET

// Now and then, instead of his usual line, one of these: about one line in 25, only for lines
// with no news in them (the time-of-day lines and a poke), so it never covers a message, a low
// battery or being shaken.
static const char* const R_RARE_L[] = {
  "Where is Shiftylab?", "Anyone seen Shiftylab?", "Shiftylab, you on mesh?",
  "DC757 roll call: Shiftylab?", "Ping Shiftylab for me?"};
constexpr uint8_t RARE_ODDS = 25;
inline bool rabbitChatKind(uint8_t k) { return k == MORNING || k == DAY || k == EVENING || k == LATE || k == POKE; }

// The holidays: on a holiday's scene, a third of his greetings and pokes are the season's.
static const char* const R_HAL_L[] = {"Trick or treat, choom!", "Spooky packets tonight.", "Boo! Did I scare you?", "Ghosts on the mesh..."};
static const char* const R_THX_L[] = {"Grateful for repeaters.", "Pass the carrots, choom.", "Thankful for good SNR.", "Happy Thanksgiving!"};
static const char* const R_XMS_L[] = {"Merry Christmas, choom!", "Snow on the antennas.", "Jingle all the packets.", "Ho ho hop!"};
static const char* const R_NYR_L[] = {"Happy New Year, choom!", "New year, new hops.", "3... 2... 1... Hop!", "Resolution: more nodes."};
static const char* const R_EAS_L[] = {"Happy Easter!", "Found any eggs yet?", "Easter bunny on duty.", "Egg hunt: still looking."};
static const char* const R_JUL_L[] = {"Happy 4th of July!", "Fireworks on the grid!", "Red, white and packets.", "Light up the mesh!"};
inline const char* rabbitHolidayLine(uint8_t scene) {
  const char* const* L = nullptr;
  switch (scene) {
    case SCENE_RHALLOWEEN: L = R_HAL_L; break;
    case SCENE_RTHANKS:    L = R_THX_L; break;
    case SCENE_RXMAS:      L = R_XMS_L; break;
    case SCENE_RNEWYEAR:   L = R_NYR_L; break;
    case SCENE_REASTER:    L = R_EAS_L; break;
    case SCENE_RJULY4:     L = R_JUL_L; break;
    default: return nullptr;
  }
  return L[random(4)];
}
