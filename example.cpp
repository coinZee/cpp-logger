#include "clogger.hpp"

int main () {
  Clogger log{"log.txt"};
  log.log("some logs");
  return 0;
}
