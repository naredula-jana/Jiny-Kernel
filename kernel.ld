SECTIONS {
        . = 0x00100000;
        .text : { * (.text); }

        . = 0x00200000;
        .data :  { * (.data); } 
}
