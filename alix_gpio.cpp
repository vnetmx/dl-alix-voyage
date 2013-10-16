/*      alix_gpio.cpp
 *      
 *      Copyright 2012 Alexander Heinlein
 *
 *      Watches the mode switch on a PCEngines ALIX board (tested on ALIX.2D13)
 *      and executes a predefined action if pressed.
 *
 *      This program forks into the background and waits for the mode switch
 *      button to be pressed. When pressed, it activates the second (=middle)
 *      led and executes the predefined action by replacing itself with the
 *      specified process (thus quitting).
 *      Note: You need to run this program as root in order to get access to
 *            the GPIO port.
 *      Note: If you want to execute the action every time the switch is
 *            pressed without quitting, just execute a fork right before the
 *            execvp call.
 *      
 *      
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 3 of the License, or
 *      (at your option) any later version.
 *      
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *      
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */
 
#include <cstdio>
#include <ctime>
#include <sys/io.h>
#include <unistd.h>
 
const unsigned short int GPIO_MODESWITCH_ADDR = 0x061b0;
const unsigned short int GPIO_MODESWITCH_BIT  = 8;
const unsigned short int GPIO_LEDTWO_ADDR     = 0x06180;
const unsigned short int GPIO_LEDTWO_BIT_ON   = 25;
const unsigned short int GPIO_LEDTWO_BIT_OFF  = 9;
const time_t   SLEEP_SECONDS     = 0;
const long int SLEEP_NANOSECONDS = 500 * 1000 * 1000; // 0.5 seconds
//const char* PRESS_ACTION[] = { "ls", "-l", "-h", NULL }; // for debugging :)
const char* PRESS_ACTION[] = { "poweroff", NULL }; // last argument must be NULL!
 
int main(int argc, char* argv[]) {
    // fork into background
    const pid_t pid = fork();
    if (pid == -1) {
        perror("Could not fork into background");
        return -1;
    } else if (pid != 0) {
        return 0; // we are the parent
    }
 
    // request access permissions for mode switch port
    // note: according to the manpage of ioperm this has to be done in the child process as
    //       permissions are not inherited by the child. but it still works when requesting
    //       permissions before the fork on my system. better don't rely on it though :).
    if (ioperm(GPIO_MODESWITCH_ADDR, 4, 1) == -1) {
        perror("Could not set access permissions for modeswitch port");
        return -1;
    }
 
    const timespec ts = { SLEEP_SECONDS, SLEEP_NANOSECONDS };    
    while (true) {
        // open port and check mode switch bit
        unsigned int val = inl(GPIO_MODESWITCH_ADDR);
        bool pressed = !(val & (1 << GPIO_MODESWITCH_BIT)); // active low, 0 = pressed
 
        if (pressed) {
            ioperm(GPIO_MODESWITCH_ADDR, 4, 0); // remove mode switch port access permissions
            if (ioperm(GPIO_LEDTWO_ADDR, 4, 1) != 1) { // request access permissions for led port
                // switching a led on requires to set one bit and to clear another one, yay
                val = inl(GPIO_LEDTWO_ADDR);
                val |= 1 << GPIO_LEDTWO_BIT_ON;
                val &= ~ (1 << GPIO_LEDTWO_BIT_OFF);
                outl(val, GPIO_LEDTWO_ADDR);
                ioperm(GPIO_LEDTWO_ADDR, 4, 0); // remove led port access permissions
            }
 
            // execute press action
            if (execvp(PRESS_ACTION[0], const_cast<char* const*>(PRESS_ACTION)) == -1) {
                perror("Could not execute press action");
                return -1;
            }
        }
 
        nanosleep(&ts, NULL);
    }
 
    return 0;
}
