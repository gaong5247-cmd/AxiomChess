#pragma once
#include <istream>
#include <string>
namespace axiom {
long long uci_number(std::istream& input,const std::string& field,long long minimum,long long maximum);
}
