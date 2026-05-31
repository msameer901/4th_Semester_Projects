#include <avr/io.h>
#include <avr/interrupt.h>
#define F_CPU 1000000UL
#include <util/delay.h>
#include <stdlib.h>

#define enable_pin         PD5
#define register_select_pin PD6

volatile int pulse = 0;
volatile int i = 0;

void send_a_command(unsigned char command);
void send_a_character(unsigned char character);
void send_a_string(const char *string_of_characters);
void barrier_up(void);
void barrier_down(void);
int main(void) {
    DDRA = 0xFF;
    DDRB = 0x01;
    DDRD = 0b11111011;
    
    PORTB=0;

    _delay_ms(50);

    GICR |= (1 << INT0);
    MCUCR |= (1 << ISC00);

    TCCR1A = 0;

    int16_t COUNTA = 0;
    char SHOWA[16];

    send_a_command(0x01); 
    _delay_ms(2);
    send_a_command(0x38); 
    _delay_us(50);
    send_a_command(0b00001111);
    _delay_us(50);

    sei();

    while (1) {
        PORTD |= (1 << PIND0);
        _delay_us(15);
        PORTD &= ~(1 << PIND0);

        COUNTA = pulse / 58;

        send_a_string("EMENENT PARKING");
        send_a_command(0x80 + 0x40 + 0);
        send_a_string("DISTANCE=");
        itoa(COUNTA, SHOWA, 10);
        send_a_string(SHOWA);
        send_a_string("cm    ");
        send_a_command(0x80 + 0);
        if(COUNTA<=10){
            barrier_up();
    }
        else{
            barrier_down();
        }
    }
    return 0;
}

ISR(INT0_vect) {
    if (i == 1) {
        TCCR1B = 0;
        pulse = TCNT1;
        TCNT1 = 0;
        i = 0;
    }

    if (i == 0) {
        TCCR1B |= (1 << CS10);
        i = 1;
    }
}

void itoa(int n, char s[], int radix) {
    int i, sign;
    
    if ((sign = n) < 0) 
        n = -n;         
    
    i = 0;
    do {
       
        s[i++] = n % radix + '0';
    } while ((n /= radix) > 0); 
    if (sign < 0)
        s[i++] = '-';
    s[i] = '\0';
    
    int length = i;
    for (i = 0; i < length / 2; ++i) {
        char temp = s[i];
        s[i] = s[length - i - 1];
        s[length - i - 1] = temp;
    }
}
void barrier_up(void)
{
        PORTB = 0x01;
        _delay_us(1500);
        PORTB = 0x00;
        _delay_ms(200);

      
}
void barrier_down(void){
      PORTB = 0x01;
        _delay_us(1000);
        PORTB = 0x00;
}
void send_a_command(unsigned char command) {
    PORTA = command;
    PORTD &= ~(1 << register_select_pin);
    PORTD |= (1 << enable_pin);
    _delay_ms(8);
    PORTD &= ~(1 << enable_pin);
    PORTA = 0;
}

void send_a_character(unsigned char character) {
    PORTA = character;
    PORTD |= (1 << register_select_pin);
    PORTD |= (1 << enable_pin);
    _delay_ms(8);
    PORTD &= ~(1 << enable_pin);
    PORTA = 0;
}

void send_a_string(const char *string_of_characters) {
    while (*string_of_characters) {
        send_a_character(*string_of_characters++);
    }
}
