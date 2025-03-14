# Guacamole Data-Diode Hardware

It could be interesting to create hardware for the data-diode. Some idea is to use an FPGA. An idea could be the following:

![fpga-data-diode-design](https://github.com/macsnoeren/guacamole-datadiode/blob/7bc6c850edbb01ea2e3cb4cf7a1a79e23f21aa8d/documentation/images/fpga-gm-data-diode-design.png)

- https://alexforencich.com/wiki/en/verilog/ethernet/start
- https://github.com/alexforencich/verilog-ethernet/tree/master
- https://bitsimnow.se/wp-content/uploads/2020/02/Data_Sheet_Bit1GUdpEthernet_A.pdf

# Verilog / HDL

```verilog
module validate_guacamole (
    input clk,
    input [7:0] data,

    output error = 0
);

reg [3:0] state = 0;
reg [15:0] length = 0;
reg opcode = 1;

always @(posedge clk)
case(state)
  0: if ( data >= "0" && data <= "9" ) begin
        length <= data - "0";
        error <= 0;
        state <= 1;
    end else error <= 1;
    
  1: if ( data >= "0" && data <= "9" ) begin
        length <= length*10 + (data - "0");
        error <= 0;
    end else if ( data == "." ) begin
        error <= 0;
        state <= 2;
    end else begin
        error <= 1;
        state <= 0;
    end
    
  2: if ( length == 0 ) begin
        if ( data == "," ) begin
            opcode <= 0;
            error <= 0;
            state <= 0;
        end else if ( data == ";" ) begin
            opcode <= 1;
            error <= 0;
            state <= 0;
        end else begin
            error <= 1;
            state <= 0;
        end
    end else length <= length - 1;
endcase

endmodule
```
