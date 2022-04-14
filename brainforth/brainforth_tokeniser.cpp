/*
This code demonstrates how to separate out the translation of Brainf*ck source 
code from its execution. We introduce an intermediate JSON format.
*/

#include <iostream>
#include <vector>
#include <fstream>
#include <map>
#include <optional>
#include <stdexcept>
#include <deque>
#include <cstdlib>
#include <cctype>

#include "json.hpp"

using namespace nlohmann;

//  Use this to turn on or off some debug-level tracing.
#define DUMP 0

//  Syntactic sugar to emphasise the 'break'.
#define break_if( E ) if ( E ) break
#define break_unless( E ) if (!(E)) break
#define continue_if( E ) if ( E ) continue
#define continue_unless( E ) if (!(E)) continue

//  Syntactic sugar to emphasise the 'return'. Usage:
//      return_if( E );
//      return_if( E )( Result )
#define return_if( E ) if (E) return
#define return_unless( E ) if (!(E)) return

template <typename T> int sgn(T val) {
    return (T(0) < val) - (val < T(0));
};

bool startsWith( std::string_view haystack, std::string_view needle ) {
    return haystack.rfind( needle, 0 ) == 0;
}

class PeekableProgramInput {
    std::istream& input;                //  The source code to be read in.
    std::deque< char > buffer;          //  Queue of characters that have been peeked.
    std::string token;
public:
    PeekableProgramInput( std::istream& input ) :
        input( input )
    {}
private:
    std::optional<char> getChar() {
        for (;;) {
            char ch = input.get();
            if ( input.good() ) {
                switch ( ch ) {
                    case '?':
                    case '!':
                    case '>':
                    case '<':
                    case '+':
                    case '-':
                    case '.':
                    case ',':
                    case '[':
                    case ']':
                        return std::optional<char>( ch );
                    default:
                        return std::optional<char>( isalnum( ch ) ? ch : ' ' );
                }
            } else {
                return std::nullopt;
            }
        }
    }
public:
    std::optional<char> peek() {
        if ( this->buffer.empty() ) {
            auto ch = getChar();
            if ( ch ) {
                this->buffer.push_back( *ch );
            }
            return ch;
        } else {
            return this->buffer.front();
        }
    }
public:
    std::optional<char> pop() {
        if ( this->buffer.empty() ) {
            return getChar();
        } else {
            char ch = this->buffer.front();
            this->buffer.pop_front();
            return ch;
        }
    }
private:
    void drop() {
        if ( this->buffer.empty() ) {
            input.get();
        } else {
            this->buffer.pop_front();
        }
    }
// public:
//     bool tryPop( char ch ) {
//         if ( this->peek() == ch ) {
//             this->drop();
//             return true;
//         } else {
//             return false;
//         }
//     }
// public:
//     bool tryPopString( std::string str ) {
//         int n = 0;
//         for ( auto ch : str ) {
//             auto actual = this->peekN( n );
//             return_if( actual != ch )( false );
//             n += 1;
//         }
//         this->buffer.erase( this->buffer.begin(), this->buffer.begin() + n );
//         return true;
//     }
private:
    void scanName() {
        for (;;) {
            auto ch = peek();
            break_unless( ch && isalnum( *ch ) );
            this->token.push_back( *ch );
            this->drop();
        }
    }
public:
    std::optional<json> nextJToken() {
        this->token.clear();
        for (;;) {
            auto ch = pop();
            return_unless( ch )( std::nullopt );
            continue_if( *ch == ' ' );
            this->token.push_back( *ch );
            if ( isalnum( *ch ) ) {
                this->scanName();
                return std::optional<json>( json( {{ "name", this->token }} ) );
            } else {
                return std::optional<json>( json( {{ "symbol", std::string( 1, *ch ) }} ) );
            }
        }
    }
};


/*
Splits Brainforth code on the standard input into a stream of tokens.
*/
int main( int argc, char * argv[] ) {
    std::vector<std::string> args(argv + 1, argv + argc);
    std::noskipws( std::cin );
    PeekableProgramInput input( std::cin );
    for (;;) {
        auto j = input.nextJToken();
        break_unless( j );
        std::cout << *j << std::endl;
    }
    exit( EXIT_SUCCESS );
}
