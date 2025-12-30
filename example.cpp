#include "clogger.hpp"

int main () {
  Clogger log{"log.txt"};
  log.log("Shits happened");
  return 0;
}
