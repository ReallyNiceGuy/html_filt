#include <string>
#include <map>
#include <vector>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <memory>
#include <cstring>
#include "html_list.hpp"

using namespace std::literals;

struct Node
{
  std::string_view value{} ;
  std::map<char, std::shared_ptr<Node>> children{};
};

Node create_search_tree();
std::vector<std::shared_ptr<Node>> create_search_vector_of_nodes();

static const std::vector<std::shared_ptr<Node>> html_entities_vector_of_nodes = create_search_vector_of_nodes();

int ch_to_idx(const char ch)
{
  int base = (ch & ~0x20) - 'A';
  if (base < 0 || base > 25) return -1;
  return base + (((ch & 0x20) >> 5) * 26);
}

std::vector<std::shared_ptr<Node>> create_search_vector_of_nodes()
{
  std::vector<std::shared_ptr<Node>> root(52);
  for (auto &&node: root)
    node = std::make_shared<Node>();
  for(auto &&item: html_entities)
  {
    auto first_char = item.first[0];

    auto result = root[ch_to_idx(first_char)];
    for(std::size_t i = 1; i < item.first.size(); ++i)
    {
        auto ch = item.first[i];
        if (!result->children.contains(ch))
        {
            result->children[ch] = std::make_shared<Node>();
        }
        result = result->children.at(ch);
    }
    result->value = item.second;
  }
  return root;
}

void dump_tree(const Node& node, int indent=0)
{
    std::cout << "{ \"";
    for(auto &&val: node.value)
    {
     std::printf("\\x%02x", (unsigned char)val);
    }
    std::cout << "\"sv,";
    std::cout << " { ";
    for(auto &&item: node.children)
    {
      std::cout << "'" << item.first << "', \n";
      for (int i=0; i < indent; ++i) std::cout << " ";
      dump_tree(*item.second, indent+1);
    }
    std::cout << " }";
    std::cout << " },\n";
    for (int i=0; i < indent-2; ++i) std::cout << " ";
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
          search_point = html_entities_vector_of_nodes[ch_idx].get();
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
          search_point = it->second.get();
          // Get next char
          continue;
        }
        state = DEFAULT;
        // Does the current Node define a valid entity?
        if (search_point->value.empty()) // No
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
