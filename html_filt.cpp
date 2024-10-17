#include <string>
#include <map>
#include <vector>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <cstring>
#include <cmath>
#include <boost/format.hpp>
#include "html_list.hpp"

using namespace std::literals;

static constexpr int LOWER_CASE_BIT{1<<5};
static constexpr int MAX_VALID_CODEPOINT{0x10ffff};

// Round up and give 1 char for a leading 0
static constexpr int MAX_DECIMAL_LEN = std::ceil(std::log10(MAX_VALID_CODEPOINT)) + 1;
static constexpr int MAX_HEX_LEN = std::ceil(std::log(MAX_VALID_CODEPOINT)/log(16)) + 1;

inline static constexpr int ucase(int ch)
{
  return ch & ~LOWER_CASE_BIT;
}

inline static constexpr int is_valid_first_entity_char(int ch)
{
  return (ucase(ch)>= 'A' && ucase(ch) <= 'Z');
}

inline static constexpr int is_valid_entity_char(int ch)
{
  return ((ch >= '0' && ch <= '9') ||
          (ucase(ch) >= 'A' && ucase(ch) <= 'Z') ||
           ch == ';');
}

inline static constexpr int is_digit(int ch)
{
  return (ch >= '0' && ch <= '9');
}

inline static constexpr int is_hex_digit(int ch)
{
   return ((ch >= '0' && ch <= '9') ||
           (ucase(ch)  >= 'A' && ucase(ch) <= 'F'));
}

inline static constexpr int is_hex_marker(int ch)
{
  return (ucase(ch) == 'X');
}

inline static constexpr int is_numeric_marker(int ch)
{
  return (ch == '#');
}

inline static constexpr int is_entity_begin(int ch)
{
  return (ch == '&');
}

inline static constexpr int is_entity_terminator(int ch)
{
  return (ch == ';');
}

inline static constexpr int is_lower_case(int ch)
{
  return (ch & LOWER_CASE_BIT);
}

static inline void puts(std::ostream& out, const std::string_view str)
{
  for(auto&& ch: str)
    out.put(ch);
}

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

constexpr inline static int index_from_char(int ch)
{
  if (is_lower_case(ch))
  {
    ch = ucase(ch) + 26;
  }
  ch -= 'A';
  return ch;
}

std::vector<Node> create_search_vector_of_nodes()
{
  std::vector<Node> root(52);
  for(auto &&item: html_entities)
  {
    const auto first_char = item.key[0];

    auto result = &root[index_from_char(first_char)];
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
  if (codepoint > MAX_VALID_CODEPOINT)
  {
    puts(out, "\ufffd"sv);
    return;
  }

  if (codepoint <= 0x7f)
  {
    out.put(static_cast<char>(codepoint));
  }
  else if (codepoint <= 0x7ff)
  {
    out.put(static_cast<char>(0xc0 | ((codepoint >> 6) & 0x1f)));
    out.put(static_cast<char>(0x80 | (codepoint & 0x3f)));
  }
  else if (codepoint <= 0xffff)
  {
    out.put(static_cast<char>(0xe0 | ((codepoint >> 12) & 0x0f)));
    out.put(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
    out.put(static_cast<char>(0x80 | (codepoint & 0x3f)));
  }
  else
  {
    out.put(static_cast<char>(0xf0 | ((codepoint >> 18) & 0x07)));
    out.put(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3f)));
    out.put(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
    out.put(static_cast<char>(0x80 | (codepoint & 0x3f)));
  }
}

void output_decimal_entity(const std::string& codepoint, std::ostream& out)
{
  char32_t i{};
  i = std::stoi(codepoint, nullptr, 10);
  unicode_to_utf8(i, out);
}

void output_hex_entity(const std::string& codepoint, std::ostream& out)
{
  char32_t i{};
  i = std::stoi(codepoint, nullptr, 16);
  unicode_to_utf8(i, out);
}

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
        // Is this a valid first character for an entity?
        if (is_valid_first_entity_char(ch))
        {
          //Yes
          state = EXPECT_CHAR;
          entity += ch;
          // Make search_point the corresponding Node for this block of entities
          search_point = &html_entities_vector_of_nodes[index_from_char(ch)];
          // Get next char
          continue;
        }
        // Invalid character
        state = DEFAULT;
        // Just copy the original content into result
        puts(out, header);
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
        puts(out, header);
        // Process this character at the end
      }
      break;
    case EXPECT_DIGIT:
      {
        if (is_digit(ch))
        {
          // Ignore a run of zeros at the beginning
          if (!(entity.size() == 1 && entity[0] == '0' && ch == '0'))
          {
            // This will avoid unbounded memory usage
            if ( entity.size() < MAX_DECIMAL_LEN)
            {
              entity += ch;
            }
          }
          // Get next char
          continue;
        }
        // Not a digit, finish processing of the decimal entity
        state = DEFAULT;
        output_decimal_entity(entity, out);
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
          // Ignore a run of zeros at the beginning
          if (!(entity.size() == 1 && entity[0] == '0' && ch == '0'))
          {
            // This will avoid unbounded memory usage
            if ( entity.size() < MAX_HEX_LEN)
            {
              entity += ch;
            }
          }
          // Get next char
          continue;
        }
        // Not a digit, finish processing of the hexadecimal entity
        state = DEFAULT;
        // Does the entity have any digits?
        if (entity.size())
        {
          // Yes
          output_hex_entity(entity, out);
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
          puts(out, header);
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
          puts(out, header);
          puts(out, entity);
          // Process this character at the end
        }
        else // Yes
        {
          // Insert the entity into the result
          puts(out, search_point->value);
          // Process this character at the end
        }
      }
      break;
    case DEFAULT:
      // Do nothing here, it is taken care below
      ;
    }

    if (ch == std::istream::traits_type::eof()) break;
    if (is_entity_begin(ch))
    {
      state = EXPECT_NUMERIC_MARKER_OR_CHAR;
      entity.clear();
      header = ch;
      // Get next char
      continue;
    }
    // Just a character, insert it on the result
    out.put(static_cast<char>(ch));
  }
}

void usage(std::ostream &out, std::string_view app)
{
  out << "Usage:\n  " << app << " [-i infile] [-o outfile] [-h]\n";
}

int main(int argc, char** argv) 
{
  std::ios_base::sync_with_stdio(false);
  enum CMDLINE_STATE
  {
    DEFAULT,
    EXPECT_IN_FILE,
    EXPECT_OUT_FILE,
  } state { DEFAULT };

  std::string infile;
  std::string outfile;
  for (int i = 1; i < argc; ++i)
  {
    switch (state)
    {
    case DEFAULT:
      if (strcmp(argv[i], "-h") == 0)
      {
        usage(std::cout, argv[0]);
        return 0;
      }
      else if (strcmp(argv[i], "-i") == 0)
      {
        if (infile.size())
        {
          std::cerr << "Multiple input files specified\n";
          usage(std::cerr, argv[0]);
          exit(-1);
        }
        state = EXPECT_IN_FILE;
      }
      else if (strcmp(argv[i], "-o") == 0)
      {
        if (outfile.size())
        {
          std::cerr << "Multiple output files specified\n";
          usage(std::cerr, argv[0]);
          exit(-1);
        }
        state = EXPECT_OUT_FILE;
      }
      else
      {
        std::cerr << "Unknown parameter: "<< argv[i] << "\n";
        usage(std::cerr, argv[0]);
        exit(-1);
      }
      break;
    case EXPECT_IN_FILE:
      infile = argv[i];
      state = DEFAULT;
      break;
    case EXPECT_OUT_FILE:
      outfile = argv[i];
      state = DEFAULT;
      break;
    }
  }
  if (state == EXPECT_IN_FILE)
  {
    std::cerr << "Missing input file\n";
    usage(std::cerr, argv[0]);
    exit(-1);
  }
  if (state == EXPECT_OUT_FILE)
  {
    std::cerr << "Missing output file\n";
    usage(std::cerr, argv[0]);
    exit(-1);
  }


  std::ifstream in;
  std::ofstream out;


  if (infile.size())
  {
    in.open(infile);
    if (!in.good())
    {
      std::cerr  << argv[0] << ": " << infile << ": " << std::strerror(errno) << "\n";
      exit(1);
    }
  }

  if (outfile.size())
  {
    out.open(outfile);
    if (!out.good())
    {
      std::cerr  << argv[0] << ": " << outfile << ": " << std::strerror(errno) << "\n";
      exit(2);
    }
  }

  std::istream *in_ptr = in.is_open() ? &in : &std::cin;
  std::ostream *out_ptr = out.is_open() ? &out : &std::cout;

  decode(*in_ptr, *out_ptr);
  if (in.is_open()) in.close();
  if (out.is_open()) out.close();
  return 0;
}
