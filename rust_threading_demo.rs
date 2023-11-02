/*
    Brainfuck interpreter written in subroutine-threaded style.
*/

use std::env;
use std::collections::BTreeMap;
use std::fs::File;
use std::io::{prelude::*, BufReader};

type OpCode = fn( &mut Engine ) -> ();

#[derive(Copy, Clone)]
union InstructionField {
    opcode: OpCode,
    operand: usize,
}

const MEMORY_SIZE: usize = 30000;

struct Engine {
    program : [ InstructionField; MEMORY_SIZE ],
    pc : usize,
    memory : [ i8; 30000 ],
    loc : usize,
}

#[allow(non_snake_case)]
fn INCR( e : &mut Engine ) {
    e.memory[ e.loc ] += 1;
    e.pc += 1;
}

#[allow(non_snake_case)]
fn DECR( e : &mut Engine ) {
    // e.memory[ e.loc ] = e.memory[ e.loc ].saturating_sub( 1 );
    e.memory[ e.loc ] -= 1;
    e.pc += 1;
}

#[allow(non_snake_case)]
fn RIGHT( e : &mut Engine ) {
    e.loc += 1;
    e.pc += 1;
}

#[allow(non_snake_case)]
fn LEFT( e : &mut Engine ) {
    e.loc -= 1;
    e.pc += 1;
}

#[allow(non_snake_case)]
fn OPEN( e : &mut Engine ) {
    if e.memory[ e.loc ] == 0 {
        unsafe {
            e.pc = e.program[ e.pc + 1 ].operand;
        }
    } else {
        e.pc += 2;
    }
}

#[allow(non_snake_case)]
fn CLOSE( e : &mut Engine ) {
    if e.memory[ e.loc ] != 0 {
        unsafe {
            e.pc = e.program[ e.pc + 1 ].operand;
        }
    } else {
        e.pc += 2;
    }
}

#[allow(non_snake_case)]
fn PUT( e : &mut Engine ) {
    let ch = e.memory[ e.loc ] as u16 as u8;
    let buf = [ ch; 1 ];
    match std::io::stdout().write( &buf ) {
        _ => {}
    }
    e.pc += 1;
}

#[allow(non_snake_case)]
fn GET( e : &mut Engine ) {
    let mut buf = [0; 1];
    match std::io::stdin().read_exact(&mut buf) {
        Ok(_) => e.memory[ e.loc ] = buf[ 0 ] as i8,
        _ => {}
    }
    e.pc += 1;
}

#[allow(non_snake_case)]
fn HALT( _e : &mut Engine ) {
    match std::io::stdout().flush() { _ => {} }; 
    std::process::exit( 0 );
}

fn load_program_from_file( filename: &String, program: &mut [ InstructionField; MEMORY_SIZE ], opcode_map: &BTreeMap< char, OpCode > ) -> Result<(), std::io::Error> {
    let input = File::open( filename )?;
    let reader = BufReader::new( input );
    let mut top: usize = 0;
    let mut indexes = Vec::<usize>::new();
    for line in reader.lines() {
        for ch in line?.chars() {
            match opcode_map.get( &ch ) {
                Some( opc ) => { 
                    program[ top ].opcode = *opc; 
                    top += 1 
                },
                None => (),
            };
            match &ch {
                '[' => { 
                    indexes.push( top );
                    top += 1;
                },
                ']' => {
                    let start = indexes.pop().expect("Unmatched closing bracket");
                    program[ start ].operand = top + 1;
                    program[ top ].operand = start + 1;
                    top += 1;
                },
                _ => {},
            }
        }
    }
    program[ top ].opcode = HALT;
    return Ok(())
}

fn run_program( e : &mut Engine ) {
    loop {
        unsafe {
            let opc: OpCode = e.program[ e.pc ].opcode;
            opc( e );
        }
    }
}

fn main() -> Result< (), std::io::Error > {
    let opcode_map: BTreeMap< char, OpCode > = BTreeMap::from( [ 
        ( '+', INCR as OpCode ), 
        ( '-', DECR as OpCode ), 
        ( '>', RIGHT as OpCode ),
        ( '<', LEFT as OpCode ),
        ( '[', OPEN as OpCode ),
        ( ']', CLOSE as OpCode ),
        ( '.', PUT as OpCode ),
        ( ',', GET as OpCode )
    ] );
    let args: Vec<String> = env::args().collect();
    for arg in &args[ 1.. ] {
        let mut e = Engine {
            program: [ InstructionField { operand: 0 }; MEMORY_SIZE ],
            pc: 0,
            memory : [ 0; MEMORY_SIZE ],
            loc: 0
        };
        load_program_from_file( &arg, &mut e.program, &opcode_map )?;
        run_program( &mut e );
    }
    return Ok(())
}
