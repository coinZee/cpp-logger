#include "cloggerv2.hpp"

int main() {
  Clogger logger{"log.txt"};
  for(int i=0; i<100000000; ++i) logger.log("log " + std::to_string(i));
  return 0;
}
