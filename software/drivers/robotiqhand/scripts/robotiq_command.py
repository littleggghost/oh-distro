#!/usr/bin/env python

import os,sys
home_dir = os.getenv("DRC_BASE")
#print home_dir
sys.path.append(home_dir + "/software/build/lib/python2.7/site-packages")
sys.path.append(home_dir + "/software/build/lib/python2.7/dist-packages")

import lcm
import robotiqhand

import baseSModel

from time import sleep, time

forceLevel = 128
speedLevel = 128

def genCommand(char):
    """Update the command according to the character entered by the user."""

    global forceLevel
    global speedLevel

    command = robotiqhand.command_t();

    #this is the default, overwrite below
    command.activate = 1
    command.do_move = 1
    command.mode = -1

    if char == 'a':
        command.do_move = 0

    if char == 'r':
        command.activate = 0
        command.do_move = 0

    if char == 'c':
        command.position = 255
        command.force = forceLevel
        command.velocity = speedLevel

    if char == 'o':
        command.position = 0
        command.force = forceLevel
        command.velocity = speedLevel

    if char == 'b':
        command.do_move = 0
        command.mode = 0
        command.position = 0
        command.velocity = speedLevel
        command.force = forceLevel

    if char == 'p':
        command.do_move = 0
        command.mode = 1
        command.position = 0
        command.velocity = speedLevel
        command.force = forceLevel

    if char == 'w':
        command.do_move = 0
        command.mode = 2
        command.position = 0
        command.velocity = speedLevel
        command.force = forceLevel

    if char == 's':
        command.do_move = 0
        command.mode = 3
        command.position = 0
        command.velocity = speedLevel
        command.force = forceLevel

    if char == 'f':
        command.do_move = 0
        speedLevel += 32
        if speedLevel > 255:
            speedLevel = 255
        print "speed level now:", speedLevel
        command.velocity = speedLevel
        command.force = forceLevel

    if char == 'l':
        command.do_move = 0
        speedLevel -= 32
        if speedLevel < 0:
            speedLevel = 0
        print "speed level now:", speedLevel
        command.velocity = speedLevel
        command.force = forceLevel

    if char == 'i':
        command.do_move = 0
        forceLevel += 32
        if forceLevel > 255:
            forceLevel = 255
        print "force level now:", forceLevel
        command.velocity = speedLevel
        command.force = forceLevel

    if char == 'd':
        command.do_move = 0
        forceLevel -= 32
        if forceLevel < 0:
            forceLevel = 0
        print "force level now:", forceLevel
        command.velocity = speedLevel
        command.force = forceLevel

    if char in [str(x) for x in range(255)]:
        command.position = int(char)
        command.force = forceLevel
        command.velocity = speedLevel

    return command


def askForCommand():
    """Ask the user for a command to send to the gripper."""

    strAskForCommand  = '-----\nAvailable commands\n\n'
    strAskForCommand += 'r: Reset\n'
    strAskForCommand += 'a: Activate\n'
    strAskForCommand += 'c: Close\n'
    strAskForCommand += 'o: Open\n'
    strAskForCommand += 'b: Basic mode\n'
    strAskForCommand += 'p: Pinch mode\n'
    strAskForCommand += 'w: Wide mode\n'
    strAskForCommand += 's: Scissor mode\n'
    strAskForCommand += '(0-255): Go to that position\n'
    strAskForCommand += 'f: Faster\n'
    strAskForCommand += 'l: Slower\n'
    strAskForCommand += 'i: Increase force\n'
    strAskForCommand += 'd: Decrease force\n'

    strAskForCommand += '-->'

    return raw_input(strAskForCommand)


def publisher(side):
    """Main loop which requests new commands and publish them on the
    SModelRobotOutput topic."""

    command_topic = side.upper() + "_ROBOTIQ_COMMAND"

    lc = lcm.LCM()

    try:
        while True:
            command = genCommand(askForCommand())

            command.utime = (time() * 1000000)

            lc.publish(command_topic, command.encode())

            sleep(0.1)

    except KeyboardInterrupt:
        pass

if __name__ == '__main__':

    if not len(sys.argv) == 2:
        print "USAGE: robotiq_command <right/left>"
        sys.exit(0)

    publisher(sys.argv[1])
