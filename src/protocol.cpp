#include "axiom/protocol.hpp"
#include <stdexcept>
namespace axiom {
long long uci_number(std::istream& input,const std::string& field,long long minimum,long long maximum) {
    std::string token;if(!(input>>token))throw std::invalid_argument("Missing UCI value: "+field);
    std::size_t used=0;long long value;
    try {value=std::stoll(token,&used);}catch(const std::exception&){throw std::invalid_argument("Invalid UCI number: "+field);}
    if(used!=token.size()||value<minimum||value>maximum)throw std::invalid_argument("Out of range UCI value: "+field);
    return value;
}
}
