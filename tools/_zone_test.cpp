#include <cstdio>
enum Choice {
  bell=0, lowShelf, highShelf, notch, bandPass, highpass, lowpass,
  tiltShelf, flatTilt, allPass, bandShelf, baxandallBass, baxandallTreble,
  brickwallHighpass, brickwallLowpass, vintageLowShelf, vintageHighShelf, numChoices
};
int typeForFrequencyZone(float frequencyHz) {
  float f = frequencyHz < 20 ? 20 : (frequencyHz > 20000 ? 20000 : frequencyHz);
  if (f < 50.0f)    return highpass;
  if (f < 150.0f)   return lowShelf;
  if (f < 8000.0f)  return bell;
  if (f < 12000.0f) return highShelf;
  return lowpass;
}
const char* names[] = { "Bell", "Lo Shelf", "Hi Shelf", "Notch", "Band Pass", "Highpass", "Lowpass",
  "Tilt Shelf", "Flat Tilt", "All Pass", "Band Shelf", "Bax Bass", "Bax Treble",
  "Brick HP", "Brick LP", "Vintage LS", "Vintage HS" };
int main() {
  float freqs[] = {30, 80, 100, 200, 1000, 5000, 9000, 15000};
  for (float f : freqs) {
    int t = typeForFrequencyZone(f);
    printf("%.0f Hz -> %d %s\n", f, t, names[t]);
  }
  printf("baxandallBass enum = %d\n", baxandallBass);
  printf("lowShelf enum = %d\n", lowShelf);
}
