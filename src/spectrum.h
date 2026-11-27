// RabbitMesh spectrum analyzer (Deck > SPECTRUM): a swept-tuned analyzer on the mesh
// radio, after WadaMesh's Spectrum app (GPL-3.0, github.com/ALLFATHER-BV/wadamesh).
//
// While it is open it BORROWS the radio: nodeLoop() (node.cpp) skips the mesh, so nothing
// re-arms receive on the home channel while the sweep hops the chip across the band. The
// mesh is deaf meanwhile; messages from the phone or the keyboard wait in the queue and
// go out when the radio is handed back. It is handed back, mesh settings restored and
// receive re-armed, when the screen is left, when the display goes dark, or when the
// screen stops being on top for a moment (spectrum::watch(), from deck::tick()).
#pragma once
#include <Arduino.h>

namespace spectrum {
  bool ownsRadio();            // nodeLoop(): true while the sweep has the radio
  void watch();                // deck::tick(): hand the radio back if the screen went away
  void open();
}
