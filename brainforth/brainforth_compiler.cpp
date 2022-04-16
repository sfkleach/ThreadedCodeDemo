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
#include <memory>

#include "json.hpp"

using namespace nlohmann;

//  Use this to turn on or off some debug-level tracing.
#define DUMP 0
#define MAIN_PROGRAM "main"

//  Syntactic sugar to emphasise the 'break'.
#define break_if( E ) if ( E ) break
#define break_unless( E ) if (!(E)) break

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


typedef struct Token {
    enum {
        END_OF_INPUT,
        SYMBOL,
        NAME
    } token_type = END_OF_INPUT;
    char symbol = '\0';
    std::string name;

public:
    bool isEndOfInput() const {
        return this->token_type == END_OF_INPUT;
    }

    const std::string toString() const {
        if ( this->token_type == END_OF_INPUT ) {
            return "<EOI>";
        } else if ( this->token_type == SYMBOL ) {
            return std::string( 1, this->symbol );
        } else if ( this->token_type == NAME ) {
            return this->name;
        } else {
            throw std::runtime_error( "Unimplemented" );
        }
    }

    bool isSymbol( char ch ) const {
        return this->token_type == SYMBOL && this->symbol == ch;
    }

    bool isntSymbol( char ch ) const {
        return this->token_type != SYMBOL || this->symbol != ch;
    }

    bool isName() const {
        return this->token_type == NAME;
    }
} Token;

class PeekableProgramInput {
    std::istream& input;                //  The source code to be read in.
    std::deque< Token > tbuffer;
public:
    PeekableProgramInput( std::istream& input ) :
        input( input )
    {}
private:
    Token fetchToken() {
        if ( input.eof() ) {
            return Token();
        } else {
            json jobj;
            input >> jobj >> std::ws;
            if ( jobj.contains( "symbol" ) ) {
                Token t;
                t.token_type = Token::SYMBOL;
                t.symbol = jobj[ "symbol" ].get<std::string>()[ 0 ];
                return t;
            } else {
                Token t;
                t.token_type = Token::NAME;
                t.symbol = 'A';
                t.name = jobj[ "name" ].get<std::string>();
                return t;
            }
        }      
    }

public:
    Token popToken() {
        if ( tbuffer.empty() ) {
            return this->fetchToken();
        } else {
            auto t = tbuffer.front();
            tbuffer.pop_front();
            return t;
        }
    }

public:
    Token peekN( size_t n ) {
        while ( this->tbuffer.size() <= n ) {
            auto tok = this->fetchToken();
            return_if( tok.isEndOfInput() )( tok );
            this->tbuffer.push_back( tok );
        }
        return this->tbuffer[ n ];
    }    

public:
    Token peekToken() {
        if ( this->tbuffer.empty() ) {
            auto tok = this->fetchToken();
            this->tbuffer.push_back( tok );
            return tok;
        } else {
            return this->tbuffer.front();
        }
    }

private:
    void drop() {
        if ( this->tbuffer.empty() ) {
            this->fetchToken();
        } else {
            this->tbuffer.pop_front();
        }
    }

public:
    bool tryPop( char ch ) {
        bool is_sym = this->peekToken().isSymbol( ch );
        if ( is_sym ) {
            this->drop();
        }
        return is_sym;
    }
public:
    bool tryPopString( std::string str ) { 
        int n = 1;
        for ( auto ch : str ) {
            auto actual = this->peekN( n );
            return_if( actual.isntSymbol( ch ) )( false );
            n += 1;
        }
        this->tbuffer.erase( this->tbuffer.begin(), this->tbuffer.begin() + n );
        return true;
    }
};

struct MoveAddMove {
    int lhs;
    int by;
    int rhs;
public:
    MoveAddMove( int lhs, int by, int rhs ) {
        this->lhs = lhs;
        this->by = by;
        this->rhs = rhs;
    }
public:
    bool matches( int L, int N, int R ) {
        return L == this->lhs && N == this->by && R == this->rhs;
    }
public:
    bool isNonZeroBalanced() {
        return ( this->lhs != 0 ) && ( ( this->lhs + this->rhs ) == 0 );
    }
};

typedef struct OpCode {
    std::string name;
    bool locIsZero;
    bool discardBeforeSetZero;
} OpCode;

typedef struct InstructionSet {
    OpCode PUSH = { "PUSH", false, false };
    OpCode POP = { "POP", false, false };
    OpCode SET_ZERO = { "SET_ZERO", true, false };
    OpCode INCR = { "INCR", false, true };
    OpCode DECR = { "DECR", false, true };
    OpCode ADD = { "ADD", false, true };
    OpCode ADD_OFFSET = { "ADD_OFFSET", false, false };
    OpCode XFR_MULTIPLE = { "XFR_MULTIPLE", false, false };
    OpCode LEFT = { "LEFT", false, false };
    OpCode RIGHT = { "RIGHT", false, false };
    OpCode SEEK_LEFT = { "SEEK_LEFT", true, false };
    OpCode SEEK_RIGHT = { "SEEK_RIGHT", true, false };
    OpCode MOVE = { "MOVE", false, false };
    OpCode OPEN = { "OPEN", false, false };
    OpCode CLOSE = { "CLOSE", true, false };
    OpCode GET = { "GET", false, false };
    OpCode PUT = { "PUT", false, false };
    OpCode CALL = { "CALL", false, false };
    OpCode SAVE = { "SAVE", false, false };
    OpCode RESTORE = { "RESTORE", false, false };
    OpCode RETURN = { "RETURN", false, false };
    OpCode HALT = { "HALT", false, false };
} InstructionSet; 

//  TODO: These should be moved to a shared header file.
const char * OPCODE = "OpCode";
const char * OPERAND = "Operand";
const char * REF = "Ref";
const char * HIGH = "High";
const char * LOW = "Low";

typedef struct CompileFlags {
    bool deadCodeRemoval = true;
    bool seekZero = true;
    bool locIsZero = true;
    bool xfrMultiple = true;
    bool unplantSuperfluousCode = true;

    void setDeadCode( bool enabled ) {
        this->deadCodeRemoval = enabled;
    }

    void setUnplantSuperfluousCode( bool enabled ) {
        this->unplantSuperfluousCode = enabled;
    }

    void setSeekZero( bool enabled ) {
        this->seekZero = enabled;
    }

    void setPruneWhenLocIsZero( bool enabled ) {
        this->locIsZero = enabled;
    }

    void setXfrMultiple( bool enabled ) {
        this->xfrMultiple = enabled;
    }

    void setAll( bool enabled ) {
        this->setDeadCode( enabled );
        this->setSeekZero( enabled );
        this->setPruneWhenLocIsZero( enabled );
        this->setXfrMultiple( enabled );
    }

    void setArg( const std::string & arg, bool enable ) {
        if ( arg == "--all" ) {
            setAll( true );
        } else if ( arg == "--none" ) {
            setAll( false );
        } else if ( arg == "--deadcode" ) {
            setDeadCode( enable );
        } else if ( arg == "--seekzero" ) {
            setSeekZero( enable );
        } else if ( arg == "--prune-if-loc-is-zero" ) {
            setPruneWhenLocIsZero( enable );
        } else if ( arg == "--xfrmultiple" ) {
            setXfrMultiple( enable );
        } else if ( arg == "--superfluous" ) {
            setUnplantSuperfluousCode( enable );
        } else {
            std::string prefix( "--no-" );
            if ( startsWith( arg, prefix ) ) {    //  is it a prefix?
                setArg( "--" + arg.substr( prefix.size() ), not enable );
            } else {
                throw std::runtime_error( "Unrecognised option: " + arg );
            }
        }
    }

    CompileFlags( const std::vector<std::string> args ) {
        for ( auto& arg : args ) {
            break_if( arg == "--" );
            setArg( arg, true );
        }
    }
} CompileFlags;

//  This class is responsible for translating the stream of source code
//  into a vector<Instruction>. It is passed a mapping from characters
//  to the addresses-of-labels, so it can plant (aka append) the exact
//  pointer to the implementing code. 
class CodePlanter {
    CompileFlags flags;
    PeekableProgramInput input;         //  The source code to be read in, stripped of comment characters.
    bool loc_is_zero = true;            //  True if, at this point in the program, the current location is guaranteed to be zero.
    const InstructionSet & instruction_set;
    std::map<std::string, json> & bindings; 
    json * program;
    std::shared_ptr< std::vector<int> > indexes;           //  Responsible for managing [ ... ] loops.
    std::vector<std::tuple<std::shared_ptr<std::vector<int>>, json *>> dump;
    
public:
    CodePlanter( 
        CompileFlags flags,
        std::istream& input_stream,
        const InstructionSet & instruction_set,
        std::map<std::string, json> & bindings 
    ) :
        flags( flags ),
        input( input_stream ),
        instruction_set( instruction_set ), 
        bindings( bindings ),
        program( &bindings[MAIN_PROGRAM] ),
        indexes( std::make_shared< std::vector<int> >() )
    {}

private:

    //  Any location manipulation ahead of a SET_ZERO is pointless; discard
    //  it. Occurs in the sierpinski.bf example.
    void unplantBeforeSetZero() {
        for (;;) {
            break_if( program->empty() );
            json op = program->back();
            break_if( not op.contains( "DiscardBeforeSetZero" ) );
            program->erase(program->end() - 1);
        }
    }

    void plantOpCode( const OpCode & opcode ) {
        program->push_back( {{ OPCODE, opcode.name }} );
        this->loc_is_zero = opcode.locIsZero;
        if ( opcode.discardBeforeSetZero ) {
            program->back()[ "DiscardBeforeSetZero" ] = true;
        }
    }

    void plantOperand( int64_t n, const OpCode & opcode ) {
        program->push_back( {{ OPERAND, n }} );
        if ( opcode.discardBeforeSetZero ) {
            program->back()[ "DiscardBeforeSetZero" ] = true;
        }
    }

    void plantRef( const std::string & name, const OpCode & opcode ) {
        program->push_back( {{ REF, name }} );
        if ( opcode.discardBeforeSetZero ) {
            program->back()[ "DiscardBeforeSetZero" ] = true;
        }
    }

    void plantOpCodeAndOperand( const OpCode & opcode, int64_t n ) {
        plantOpCode( opcode );
        plantOperand( n, opcode );
    }

    void plantOpCodeAndRef( const OpCode & opcode, const std::string & name ) {
        plantOpCode( opcode );
        plantRef( name, opcode );
    }

    void plantDyad( int32_t hi, int32_t lo ) {
        program->push_back( { { HIGH, hi }, { LOW, lo } } );
    }

    void plantOPEN() {
        if ( DUMP ) std::cerr << "OPEN" << std::endl;
        plantOpCode( instruction_set.OPEN );
        //  If we are dealing with loops, we plant the absolute index of the
        //  operation in the program we want to jump to. This can be improved
        //  fairly easily.
        indexes->push_back( program->size() );
        program->push_back( nullptr );         //  Dummy value, will be overwritten.
    }

    void plantCLOSE() {
        if ( DUMP ) std::cerr << "CLOSE" << std::endl;
        plantOpCode( instruction_set.CLOSE );
        //  If we are dealing with loops, we plant the absolute index of the
        //  operation in the program we want to jump to. This can be improved
        //  fairly easily.
        size_t end = program->size();
        size_t start = indexes->back();
        size_t delta = end - start;
        if ( (*program)[ start ] != nullptr ) {
            std::string j = (*program)[start];
            throw std::runtime_error( "Patch on CLOSE " + j );
        }
        indexes->pop_back();
        (*program)[ start ] = {{ OPERAND, delta }};     //  Overwrite the dummy value.
        plantOperand( -delta, instruction_set.CLOSE );
    }

    void plantPUT() {
        if ( DUMP ) std::cerr << "PUT" << std::endl;
        plantOpCode( instruction_set.PUT );
    }

    void plantGET() {
        if ( DUMP ) std::cerr << "GET" << std::endl;
        plantOpCode( instruction_set.GET );
    }

    void plantSEEK_LEFT() {
        if ( DUMP ) std::cerr << "SEEK_LEFT" << std::endl;
        plantOpCode( instruction_set.SEEK_LEFT );
    }

    void plantSEEK_RIGHT() {
        if ( DUMP ) std::cerr << "SEEK_RIGHT" << std::endl;
        plantOpCode( instruction_set.SEEK_RIGHT );
    }

    void plantMOVE( int n ) {
        if ( n == 1 ) {
            if ( DUMP ) std::cerr << "RIGHT" << std::endl;
            plantOpCode( instruction_set.RIGHT );
        } else if ( n == -1 ) {
            if ( DUMP ) std::cerr << "LEFT" << std::endl;
            plantOpCode( instruction_set.LEFT );
        } else if ( n != 0 ) {
            if ( DUMP ) std::cerr << "MOVE " << n << std::endl;
            plantOpCodeAndOperand( instruction_set.MOVE, n );
        }
    }

    void plantADD( int n ) {
        if ( n == 1 ) {
            if ( DUMP ) std::cerr << "INCR" << std::endl;
            plantOpCode( instruction_set.INCR);
        } else if ( n == -1 ) {
            if ( DUMP ) std::cerr << "DECR" << std::endl;
            plantOpCode( instruction_set.DECR );
        } else if ( n != 0 ) {
            if ( DUMP ) std::cerr << "ADD " << n << std::endl;
            plantOpCodeAndOperand( instruction_set.ADD, n );
        }
    }

    int scanAdd( int n ) {
        for (;;) {
            if ( input.tryPop( '+' ) ) {
                n += 1;
            } else if ( input.tryPop( '-' ) ) {
                n -= 1;
            } else {
                break;
            }
        }
        return n;
    }

    int scanMove( int n ) {
        for (;;) {
            if ( input.tryPop( '>' ) ) {
                n += 1;
            } else if ( input.tryPop( '<' ) ) {
                n -= 1;
            } else {
                break;
            }
        }
        return n;
    }

    void plantADD_OFFSET( int32_t offset, int32_t by ) {
        if ( DUMP ) std::cerr << "ADD_OFFSET offset=" << offset << " by=" << by << std::endl;
        plantOpCode( instruction_set.ADD_OFFSET );
        plantDyad( offset, by );
    }

    void plantXFR_MULTIPLE( int32_t offset, int32_t by ) {
        if ( DUMP ) std::cerr << "XFR_MULTIPLE offset=" << offset << " by=" << by << std::endl;
        plantOpCode( instruction_set.XFR_MULTIPLE );
        plantDyad( offset, by );
    }

    void plantMoveAddMove( const MoveAddMove & mim ) {
        if ( mim.by == 0 ) {
            if ( mim.rhs == 0 ) {
                plantMOVE( mim.lhs );
            } else if ( mim.lhs == 0 ) {
                const MoveAddMove nextmam = scanMoveAddMove( mim.rhs );
                plantMoveAddMove( nextmam );
            } else {
                const MoveAddMove nextmam = scanMoveAddMove( mim.lhs + mim.rhs );
                plantMoveAddMove( nextmam );
            }
        } else if (
            ( mim.lhs != 0 && mim.rhs != 0 ) &&     
            ( sgn( mim.lhs ) != sgn( mim.rhs ) ) //  And they have opposite signs
        ) {
            int abs_lhs = abs( mim.lhs ); 
            int abs_rhs = abs( mim.rhs );
            if ( abs_lhs == abs_rhs ) {
                plantADD_OFFSET( mim.lhs, mim.by );
            } else if ( abs_lhs > abs_rhs ) {
                plantMOVE( sgn( mim.lhs ) * ( abs_lhs - abs_rhs ) );
                plantADD_OFFSET( sgn( mim.lhs ) * abs_rhs, mim.by );
            } else /* if ( abs_lhs < abs_rhs ) */ {
                plantADD_OFFSET( mim.lhs, mim.by );
                const MoveAddMove nextmam = scanMoveAddMove( sgn( mim.rhs ) * ( abs_rhs - abs_lhs ) );
                plantMoveAddMove( nextmam );
           }
        } else {
            plantMOVE( mim.lhs );
            plantADD( mim.by );
            const MoveAddMove nextmam = scanMoveAddMove( mim.rhs );
            plantMoveAddMove( nextmam );
        }
    }

    void plantPUSH() {
        if ( DUMP ) std::cerr << "PUSH" << std::endl;
        plantOpCode( instruction_set.PUSH );
    }

    void plantPOP() {
        if ( DUMP ) std::cerr << "POP" << std::endl;
        plantOpCode( instruction_set.POP );
    }

    void plantSetZero() {
        if ( DUMP ) std::cerr << "SET_ZERO" << std::endl;
        plantOpCode( instruction_set.SET_ZERO );
    }

    MoveAddMove scanMoveAddMove( int initial ) {
        int move_lhs = scanMove( initial );
        int n = scanAdd( 0 );
        int move_rhs = scanMove( 0 );   
        return MoveAddMove( move_lhs, n, move_rhs );
    }

    void plantCALL( const std::string & name ) {
        plantOpCodeAndRef( instruction_set.CALL, name );
    }

    void plantSAVE() {
        plantOpCode( instruction_set.SAVE );
    }

    void plantRESTORE() {
        plantOpCode( instruction_set.RESTORE );
    }

    bool plantExpr() {
        auto tok = input.popToken();
        return_if( tok.isEndOfInput() )( false );
        char ch = tok.symbol;

        switch ( ch ) {
            case '+':
                plantADD( scanAdd( 1 ) );
                break;
            case '-':
                plantADD( scanAdd( -1 ) );
                break;
            case '?':
                plantPUSH();
                break;
            case '!':
                plantPOP();
                break;
            case '>':
            case '<':
                {
                    MoveAddMove mim = scanMoveAddMove( tok.isSymbol('>') ? 1 : -1 );
                    plantMoveAddMove( mim );
                }
                break;
            case '[':
                if ( this->loc_is_zero && flags.deadCodeRemoval ) {
                    //  Putting comments inside [ ... ] when the location is 
                    //  known to be zero is a frequent feature of Brainf*ck 
                    //  programs. This enables us to delete the comment.
                    int nesting = 1;
                    for (;;) {
                        tok = input.popToken();
                        if ( tok.isEndOfInput() ) {
                            throw std::runtime_error( "No matching brace" );
                        }
                        if ( tok.isSymbol('[') ) {
                            nesting += 1;
                        } else if ( tok.isSymbol(']') ) {
                            nesting -= 1;
                        }
                        break_if( nesting == 0 );
                    }
                } else {
                    MoveAddMove mim = scanMoveAddMove( 0 );
                    bool bump = mim.matches( 0, 1, 0 ) || mim.matches( 0, -1, 0 );
                    if ( bump && flags.locIsZero && input.tryPop( ']' ) ) {
                        unplantBeforeSetZero();
                        plantSetZero();
                    } else if ( flags.seekZero && mim.matches( 1, 0, 0 ) && input.tryPop( ']') ) {
                        plantSEEK_RIGHT();
                    } else if ( flags.seekZero && mim.matches( -1, 0, 0 ) && input.tryPop( ']') ) {
                        plantSEEK_LEFT();
                    } else if ( flags.xfrMultiple && mim.isNonZeroBalanced() && input.tryPopString( "-]" ) ) {
                        plantXFR_MULTIPLE( mim.lhs, mim.by );
                    } else {
                        plantOPEN();
                        plantMoveAddMove( mim );
                    }   
                }
                break;
            case ']':
                plantCLOSE();
                break;
            case '.':
                plantPUT();
                break;
            case ',':
                plantGET();
                break;
            case '$':
                plantSAVE();
                break;
            case '#':
                plantRESTORE();
                break;
            case ':':
                //  The next token must be a name.
                tok = input.popToken();
                if ( tok.isName() ) {
                    this->dump.push_back( { indexes, program } );
                    program = &this->bindings[ tok.name ];
                    indexes = std::make_shared< std::vector<int> >();
                } else {
                    throw std::runtime_error( "Unexpected token following ':' (" + tok.toString() + ")" );
                }
                break;
            case ';':
                plantOpCode( instruction_set.RETURN );
                std::tie( indexes, program ) = this->dump.back();
                this->dump.pop_back();
                break;
            case 'A':
                // std::cerr << "NAME " << tok.toString() << std::endl;
                plantCALL( tok.name );                
                break;
            default:
                throw std::runtime_error( "Unhandled opcode: " + tok.toString() );
        }
        return true;
    }

public:
    void plantProgram() {
        while ( plantExpr() ) {}
        plantOpCode( instruction_set.HALT );
    }
};

/*
Compiles Brainforth code on the standard input into a JSON array of 
CISC instructions.
*/
int main( int argc, char * argv[] ) {
    std::vector<std::string> args(argv + 1, argv + argc);
    CompileFlags flags( args );

    std::map<std::string, json> bindings;
    const InstructionSet instruction_set;

    CodePlanter planter( flags, std::cin, instruction_set, bindings );
    planter.plantProgram();

    for ( auto & [ name, code ] : bindings ) {
        for ( auto & i : code ) {
            i.erase( "DiscardBeforeSetZero" );
        }
    }

    json main = bindings;

    std::cout << main.dump(4) << std::endl;
    return( EXIT_SUCCESS );
}
