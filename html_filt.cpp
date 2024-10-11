#include <string>
#include <iostream>
#include <fstream>
#include <cstring>
#include <array>
#include "html_list.hpp"

using namespace std::literals;

inline static constexpr int is_valid_first_entity_char(int ch)
{
  return ((ch >= 'A' && ch <= 'Z') ||
          (ch >= 'a' && ch <= 'z'));
}

inline static constexpr int is_valid_entity_char(int ch)
{
  return ((ch >= '0' && ch <= '9') ||
          (ch >= 'A' && ch <= 'Z') ||
          (ch >= 'a' && ch <= 'z') ||
           ch == ';');
}

inline static constexpr int is_digit(int ch)
{
  return (ch >= '0' && ch <= '9');
}

inline static constexpr int is_hex_digit(int ch)
{
   return ((ch >= '0' && ch <= '9') ||
           ((ch & ~0x20)  >= 'A' && (ch & ~0x20) <= 'F'));
}

inline static constexpr int is_hex_marker(int ch)
{
  return ((ch & ~0x20) == 'X');
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
  return (ch & 0x20);
}


constexpr int partial_compare(const std::string_view item, const std::string_view partial, int ch)
{
  int result = item.compare(0, partial.size(), partial);
  if (result == 0)
  {
    if (item.size() > partial.size())
    {
      return (unsigned char)item[partial.size()] - ch;
    }
    return -1;
  }
  return result;
}

constexpr int binary_search(const std::string_view partial, int ch, int &orig_low, int &orig_high)
{
  // It is very likely that the item is the first one
  // test it first
  if (orig_low <= orig_high && partial_compare(html_entities[orig_low].key, partial, ch) == 0)
  {
    return 1;
  }

  int low = orig_low;
  int high = orig_high;

  while (low <= high) {
    int mid = low + (high - low) / 2;

    int comp_result = partial_compare(html_entities[mid].key, partial, ch);
    if (comp_result == 0)
    {
      orig_low = mid;
      orig_high = high;
      return 1; // found a target element
    }
    else if (comp_result < 0)
    {
        low = mid + 1; // search in the upper half
    }
    else
    {
        high = mid - 1; // search in the lower half
    }
  }
  return 0; // target not found
}

consteval auto create_index_list()
{
  std::array<int, 53> limits;

  int ch{-1};
  int idx{0};
  for(int i = 0; i < html_entities_size; ++i)
  {
    if (ch != html_entities[i].key[0])
    {
      limits[idx] = i;
      ++idx;
      ch = html_entities[i].key[0];
    }
  }
  limits[52] = html_entities_size;
  return limits;
};

constexpr static auto initial_limits_for_binary_search = create_index_list();

int find_first_of(int ch, int &low, int &high)
{
  constexpr int offset_for_lowercase = 26 - 32;

  if (is_valid_first_entity_char(ch))
  {
    if (is_lower_case(ch))
    {
      ch += offset_for_lowercase;
    }
    ch -= 'A';
    low = initial_limits_for_binary_search[ch];
    high = initial_limits_for_binary_search[ch + 1] - 1;
    return 1;
  }
  return 0;
}

constexpr int find_first_of(const std::string_view partial, int ch, int &low, int &high)
{
  if (is_valid_entity_char(ch))
  {
    // If limits are the same
    if (low == high)
    {
      // Just compare the last character of the entity, everything else already matched before
      return html_entities[low].key.size() > partial.size() && html_entities[low].key[partial.size()] == ch;
    }

    int new_low = low;
    if (binary_search(partial, ch, new_low, high))
    {
      // Found a target element
      int new_high{new_low - 1};
      int maybe_low{low};
      // Only try to find a lower one if the limits are different
      while (new_low != high && binary_search(partial, ch, maybe_low, new_high))
      {
        // Found another target element, lower in the list
        new_low = maybe_low;
        new_high = maybe_low - 1;
        maybe_low = low;
      }
      low = new_low;
      return 1;
    }
  }
  return 0;
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
  int low{};
  int high{};

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
        low = 0;
        high = html_entities_size - 1;
        // Is this a valid first character for an entity?
        if (find_first_of(ch, low, high))
        {
          //Yes
          state = EXPECT_CHAR;
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
        // Does this character is part of a valid entity?
        if (find_first_of(entity, ch, low, high))
        {
          entity += ch;
          // Get next char
          continue;
        }
        state = DEFAULT;
        // Is this entity complete?
        if (html_entities[low].key != entity) // No
        {
          // Just copy the original content into the result
          out << header;
          out << entity;
          // Process this character at the end
        }
        else // Yes
        {
          // Insert the entity into the result
          out << html_entities[low].value;
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
