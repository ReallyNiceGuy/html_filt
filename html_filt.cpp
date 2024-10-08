#include <string>
#include <map>
#include <vector>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <cstring>
#include <boost/format.hpp>
#include "html_list.hpp"

using namespace std::literals;
struct Node;
std::ostream& dump_map(std::ostream& out, const std::map<char, Node>& the_map, int indent=0);
std::ostream& dump_node(std::ostream& out, const Node& node, int indent=0);

struct Node
{
  const char* value{nullptr};
  std::map<char, Node> children{};
};

std::ostream& dump_node(std::ostream& out, const Node& node, int indent)
{
  out << "{";
  if (node.value)
  {
    out << "\"";
    for (int i = 0; node.value[i] !=0 ; ++i)
    {
      out << boost::format("\\x%|02x|") % ((int)node.value[i]&0xff);
    }
    out << "\"";
  }
  else
  {
    out << "nullptr";
  }
  out << ", ";
  dump_map(out, node.children, indent+2);
  out << "}";
  return out;
}

std::ostream& dump_map(std::ostream& out, const std::map<char, Node>& the_map, int indent)
{
  out << "{";
  if (the_map.size())
  {
    out << "\n";
    auto last = --the_map.end();
    for(auto it = the_map.begin(); it != the_map.end(); ++it)
    {
      for (int i=0;i<indent;++i) out << " ";
      out << "{'" << it->first << "', ";
      dump_node(out, it->second, indent);
      out << "}";
      if (it != last)
      {
        out << ",\n";
      }
    }
  }
  out << "}";
  return out;
}

std::ostream& dump_vector_of_nodes(std::ostream& out, const std::vector<Node>& vec, int indent=0)
{
  out << "{\n";
  char i=0;
  for(auto&& item: vec)
  {
    out << "//";
    if (i<26)
    {
      out << (char)('A'+i);
    }
    else
    {
      out << (char)(71+i);
    }
    out << "\n";
    for (int i=0;i<indent;++i) out << " ";
    dump_node(out, item, indent);
    out<< ",\n";
    ++i;
  }
  out << "};\n";
  return out;
}

Node create_search_tree();
std::vector<Node> create_search_vector_of_nodes();

static const std::vector<Node> html_entities_vector_of_nodes = create_search_vector_of_nodes();

int ch_to_idx(const char ch)
{
  int base = (ch & ~0x20) - 'A';
  if (base < 0 || base > 25) return -1;
  return base + (((ch & 0x20) >> 5) * 26);
}

std::vector<Node> create_search_vector_of_nodes()
{
  std::vector<Node> root(52);
  for(auto &&item: html_entities)
  {
    const auto first_char = item.key[0];

    auto result = &root[ch_to_idx(first_char)];
    for(std::size_t i = 1; item.key[i] != 0; ++i)
    {
        const auto ch = item.key[i];
        result = &result->children[ch];
    }
    result->value = item.value;
  }
  return root;
}

void unicode_to_utf8(char32_t codepoint, std::ostream& out)
{
  if (codepoint > 0x10ffff)
  {
    out << "\ufffd"sv;
    return;
  }

  if (codepoint <= 0x7f)
  {
    out << static_cast<char>(codepoint);
  }
  else if (codepoint <= 0x7ff)
  {
    out << static_cast<char>(0xc0 | ((codepoint >> 6) & 0x1f));
    out << static_cast<char>(0x80 | (codepoint & 0x3f));
  }
  else if (codepoint <= 0xffff)
  {
    out << static_cast<char>(0xe0 | ((codepoint >> 12) & 0x0f));
    out << static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f));
    out << static_cast<char>(0x80 | (codepoint & 0x3f));
  }
  else
  {
    out << static_cast<char>(0xf0 | ((codepoint >> 18) & 0x07));
    out << static_cast<char>(0x80 | ((codepoint >> 12) & 0x3f));
    out << static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f));
    out << static_cast<char>(0x80 | (codepoint & 0x3f));
  }
}

void insert_decimal_entity_into_result(const std::string& codepoint, std::ostream& out)
{
  char32_t i{0xffffff};
  if (codepoint.size() < 8)
  {
    i = std::stoi(codepoint, nullptr, 10);
  }
  unicode_to_utf8(i, out);
}

void insert_hex_entity_into_result(const std::string& codepoint, std::ostream& out)
{
  char32_t i{0xffffff};
  if (codepoint.size() < 6)
  {
    i = std::stoi(codepoint, nullptr, 16);
  }
  unicode_to_utf8(i, out);
}

#define is_digit(ch_) (ch_ >= '0' && ch_ <= '9')

#define is_hex_digit(ch_) \
   ((ch_ >= '0' && ch_ <= '9') || \
   ((ch_ & ~0x20)  >= 'A' && (ch_ & ~0x20) <= 'F'))

#define is_hex_marker(ch_) ((ch_ & ~0x20) == 'X')

#define is_numeric_marker(ch_) (ch_ == '#')

#define is_entity_begin(ch_) (ch_ == '&')

#define is_entity_terminator(ch_) (ch_ == ';')

void decode(std::istream &in, std::ostream &out)
{
  enum DECODE_STATE
  {
    DEFAULT,
    EXPECT_NUMERIC_MARKER_OR_CHAR,
    EXPECT_CHAR,
    EXPECT_HEX_MARK_OR_DIGIT,
    EXPECT_HEX_DIGIT,
    EXPECT_DIGIT,
  } state{DEFAULT};
  std::string header;
  std::string entity;
  Node const * search_point{};

  while(true)
  {
    int ch = in.get();

    switch (state)
    {
    case EXPECT_NUMERIC_MARKER_OR_CHAR:
      {
        if (is_numeric_marker(ch))
        {
          state = EXPECT_HEX_MARK_OR_DIGIT;
          header += ch;
          // Get next char
          continue;
        }
        auto ch_idx = ch_to_idx(ch);
        // Is this a valid first character for an entity?
        if (ch_idx != -1)
        {
          //Yes
          state = EXPECT_CHAR;
          entity += ch;
          // Make search_point the corresponding Node for this block of entities
          search_point = &html_entities_vector_of_nodes[ch_idx];
          // Get next char
          continue;
        }
        // Invalid character
        state = DEFAULT;
        // Just copy the original content into result
        out << header;
        // Process this character at the end
      }
      break;
    case EXPECT_HEX_MARK_OR_DIGIT:
      {
        if (is_hex_marker(ch))
        {
          state = EXPECT_HEX_DIGIT;
          header += ch;
          // Get next char
          continue;
        }
        if (is_digit(ch))
        {
          state = EXPECT_DIGIT;
          entity += ch;
          // Get next char
          continue;
        }
        // Invalid character
        state = DEFAULT;
        // Just copy the original content into result
        out << header;
        // Process this character at the end
      }
      break;
    case EXPECT_DIGIT:
      {
        if (is_digit(ch))
        {
          state = EXPECT_DIGIT;
          entity += ch;
          // Get next char
          continue;
        }
        // Not a digit, finish processing of the decimal entity
        state = DEFAULT;
        insert_decimal_entity_into_result(entity, out);
        if (is_entity_terminator(ch))
        {
          // Get next char
          continue;
        }
        // Process this character at the end unless it is a entity terminator char
      }
      break;
    case EXPECT_HEX_DIGIT:
      {
        if (is_hex_digit(ch))
        {
          entity += ch;
          // Get next char
          continue;
        }
        // Not a digit, finish processing of the hexadecimal entity
        state = DEFAULT;
        // Does the entity have any digits?
        if (entity.size())
        {
          // Yes
          insert_hex_entity_into_result(entity, out);
          if (is_entity_terminator(ch))
          {
            // Get next char
            continue;
          }
          // Process this character at the end unless it is a entity terminator char
        }
        else
        {
          // No
          // Just copy the original content into the result
          out << header;
          // Process this character at the end
        }
      }
      break;
    case EXPECT_CHAR:
      {
        auto it = search_point->children.find(ch);
        // Does this character appear under this Node?
        if (it != search_point->children.end())
        {
          entity += ch;
          // Make search_point be the underlying Node
          search_point = &it->second;
          // Get next char
          continue;
        }
        state = DEFAULT;
        // Does the current Node define a valid entity?
        if (search_point->value == nullptr) // No
        {
          // Just copy the original content into the result
          out << header;
          out << entity;
          // Process this character at the end
        }
        else // Yes
        {
          // Insert the entity into the result
          out << search_point->value;
          // Process this character at the end
        }
      }
      break;
    case DEFAULT:
      // Do nothing here, it is taken care below
      break;
    }

    if (ch == std::istream::traits_type::eof()) break;
    if (is_entity_begin(ch))
    {
      state = EXPECT_NUMERIC_MARKER_OR_CHAR;
      entity.clear();
      header.clear();
      header += ch;
      // Get next char
      continue;
    }
    // Just a character, insert it on the result
    out << static_cast<char>(ch);
  }
}

void usage(std::ostream &out, std::string_view app)
{
  out << "Usage:\n  " << app << " [infile [outfile]]\n";
}

int main(int argc, char** argv) 
{
  if (argc == 1)
  {
      decode(std::cin, std::cout);
  }
  else if (argc == 2 || argc == 3)
  {
    if ("-h"sv ==  argv[1] || (argc == 3 && "-h"sv == argv[2]))
    {
      usage(std::cout, argv[0]);
      return 0;
    }
    if ("-d"sv ==  argv[1] || (argc == 3 && "-d"sv == argv[2]))
    {
      dump_vector_of_nodes(std::cout, html_entities_vector_of_nodes, 2);
      return 0;
    }
    auto in = std::ifstream(argv[1]);
    if (in.good())
    {
      if (argc == 3)
      {
        std::ofstream out(argv[2]);
        if (!out.good())
        {
          std::cerr  << argv[0] << ": " <<argv[2] << ": " << std::strerror(errno) << "\n";
          return 2;
        }
        decode(in, out);
      }
      else
        decode(in, std::cout);
    }
    else
    {
      std::cerr  << argv[0] << ": " <<argv[1] << ": " << std::strerror(errno) << "\n";
      return 1;
    }
  }
  else
  {
    std::cerr  << argv[0] << ": Too many parameters\n\n";
    usage(std::cerr, argv[0]);
    return 3;
  }
  return 0;
}
