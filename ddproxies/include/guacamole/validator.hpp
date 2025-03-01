/*
Copyright (C) 2025 Maurice Snoeren

This program is free software: you can redistribute it and/or modify it under the terms of 
the GNU General Public License as published by the Free Software Foundation, version 3.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; 
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with this program.
If not, see https://www.gnu.org/licenses/.
*/
#pragma once

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// State that is used to validate the protocol.
enum class PROTOCOL_VALIDATOR_STATE {
    START,
    LENGTH,
    VALUE,
};

// Instructions consist of the elements opcode and argument. In order to identify wether it is
// an opcode or an argument this enum is used.
enum class PROTOCOL_VALIDATOR_ELEMENT {
    OPCODE,
    ARGUMENT,
};

/*
 * This class implements the protocol validation for the Guacamole protocol. It is fed the
 * protocol messages that are send and received by the Guacamole web client and guacd server.
 * It will give indications when the protocol is not valid. Furthermoe, it is also possible
 * to remove forbidden elements from the protocol. This class can be used to validate one
 * stream between the Guacamole server.
 */
class ProtocolValidator {
private:
    PROTOCOL_VALIDATOR_STATE state;
    PROTOCOL_VALIDATOR_ELEMENT element;

    std::string stringLength;
    std::string processedData;
    std::string opcode;
    long valueLength;

protected:
    void processByte (char c) {
        this->processedData += c;

        switch (this->state) {
        case PROTOCOL_VALIDATOR_STATE::START: // Search for a digit
            if ( c >= '0' and c <= '9' ) {
                this->stringLength = this->stringLength + c;
                this->state = PROTOCOL_VALIDATOR_STATE::LENGTH;
            } else {
                std::cout << "ERROR(0): " << this->processedData << std::endl;
                this->processedData == "";
                this->state = PROTOCOL_VALIDATOR_STATE::START;
            }
            break;
        
        case PROTOCOL_VALIDATOR_STATE::LENGTH: // Read the full length until dot
            if ( c >= '0' and c <= '9' ) {
                this->stringLength = this->stringLength + c;
            } else if ( c == '.' ) { // We found the full length
                this->valueLength = std::stol(this->stringLength.c_str());
                std::cout << "Found length: " << this->valueLength << std::endl;
                this->state = PROTOCOL_VALIDATOR_STATE::VALUE;
            } else {
                std::cout << "ERROR(1): " << this->processedData << std::endl;
                this->processedData == "";
                this->state = PROTOCOL_VALIDATOR_STATE::START;
            }
            break;
        
        case PROTOCOL_VALIDATOR_STATE::VALUE:
            if ( this->valueLength == 0 ) { // Read the value
                if ( c == ',' or c == ';' ) {
                    if ( this->element == PROTOCOL_VALIDATOR_ELEMENT::OPCODE ) {
                        std::cout << "Processing opcode: " << this->opcode << std::endl;
                    }
                    this->element = ( c == ';' ? PROTOCOL_VALIDATOR_ELEMENT::OPCODE : PROTOCOL_VALIDATOR_ELEMENT::ARGUMENT );
                    this->opcode = "";
                    this->processedData == "";
                    this->state = PROTOCOL_VALIDATOR_STATE::START;
                } else {
                    std::cout << "ERROR(2.1): " << this->processedData << std::endl;
                    this->processedData == "";
                    this->state = PROTOCOL_VALIDATOR_STATE::START;
                }
            } else {
                this->valueLength--;
                if ( this->element == PROTOCOL_VALIDATOR_ELEMENT::OPCODE ) {
                    this->opcode += c;
                }
            }
            break;

        default:
            std::cout << "ERROR(4): Unknown state" << std::endl;
            this->state = PROTOCOL_VALIDATOR_STATE::START;
        }
    }

public:
    ProtocolValidator (): state(PROTOCOL_VALIDATOR_STATE::START), element(PROTOCOL_VALIDATOR_ELEMENT::OPCODE), stringLength(""), processedData(""), opcode(""), valueLength(0) {

    }

    ~ProtocolValidator () {

    }

    void processData (char* data, ssize_t dataLength) {
        for ( ssize_t i=0; i < dataLength; i++ ) {
            this->processByte(data[i]);
        }
    }



};

/* python validator

def process_data(s, data):
    for b in data:
        process_byte(s, b)

# Validate protocol
# todo: based on opcode data length could be validated
# todo: the opcodes retrieved can be checked whether it is belonging to the real opcode list of guacamole
def process_byte(s, b):
    global fsm_state
    global length_value_string
    global length_value
    global processed_data
    global processing_opcode
    global opcode

    processed_data=processed_data+b
    
    if fsm_state == 0: # get the length
        processed_data=""
        if b >= '0' and b <='9':
            length_value_string=b
            fsm_state = 1
        else:
            print("ERROR(0) '"  + processed_data + "'")
            fsm_state = 0

    elif fsm_state == 1: # read the length until dot .
        if b >= '0' and b <='9':
            length_value_string=length_value_string+b
        elif b == '.':
            if length_value_string.isnumeric():
                length_value=int(length_value_string)
                #print("LENGTH: '" + length_value_string + "'")
                fsm_state = 2
            else:
                print("ERROR(1.1) '"  + processed_data + "'")
                processed_data=""
                fsm_state = 0
        else:
            print("ERROR(1)")
            fsm_state = 0
        
    elif fsm_state == 2: # read the value until counter is zero
        if length_value == 0:
            if b == ',' or b == ';': # ready and start with length again
                if processing_opcode:
                    print("OPCODE: (" + s + "): '" + opcode + "'")
                processing_opcode = b == ';'
                opcode=""
                #print("CORRECT!")
                fsm_state = 0
            else:
                print("ERROR(2.1) '"  + processed_data + "'")
                processed_data=""
                fsm_state = 0
        else:
            #if b != ',' and b != '.' and b != ';':
            length_value=length_value-1
            if processing_opcode:
                opcode=opcode+b
            #else:
            #    print("ERROR(2) '"  + processed_data + "'")
            #    processed_data=""
            #    fsm_state = 0
    else:
        print("ERROR UNKNOWN STATE")
        fsm_state=0

*/