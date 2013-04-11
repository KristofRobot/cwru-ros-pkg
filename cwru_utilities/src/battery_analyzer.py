#!/usr/bin/env python

#####################################################################
# Software License Agreement (BSD License)
#
# Copyright (c) 2012, Edward Vneator, Case Western Reserve University
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
#  * Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
#  * Redistributions in binary form must reproduce the above
#    copyright notice, this list of conditions and the following
#    disclaimer in the documentation and/or other materials provided
#    with the distribution.
#  * Neither the name of Case Western Reserve University nor the names 
#    of its contributors may be used to endorse or promote products 
#    derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
# FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
# COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
# BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
# ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.

__author__ = "esv@case.edu (Edward Venator)"

import roslib; roslib.load_manifest("cwru_utilities")
import rospy

#DT
from geometry_msgs.msg import Twist

#Battery Voltage
from cwru_base.msg import PowerState

#Arm
from arm_navigation_msgs.msg import MoveArmAction
from abby_arm_actions.stow_arm import StowArm
from abby_arm_actions.store_object import StoreObject

#Gripper
from abby_gripper.srv import gripper, gripperRequest

#Python
import math
import time
from datetime import datetime
import os

class BatteryAnalyzer:
    def __init__(self, test="idle", output="screen", ):
       	if output == "file":
       	    try:
       	        os.mkdir(os.path.expanduser("~/cwru_battery_analyzer/"))
       	    except Exception:
       	    	pass
       	    dateTime = datetime.now().strftime("%Y-%m-%d_%H%M%S")
       	    self.file_handle = open(os.path.expanduser("~/cwru_battery_analyzer/"+dateTime+"_"+test+".csv"), "w")
            self.file_handle.write('Time,Battery Voltage,13.8v Rail Voltage,cRIO Voltage\n')
       	    self.file_output = True
        else:
            self.file_output = False
        
        if test == "idle":
        	self.behavior = self.idle
        elif test == "drivetrain":
            self.commandPublisher = rospy.Publisher("cmd_vel", Twist)
            self.twistMsg = Twist()
            self.twistMsg.angular.x = 0.0
            self.twistMsg.angular.y = 0.0
            self.twistMsg.angular.z = 0.0
            self.twistMsg.linear.x = 0.0
            self.twistMsg.linear.y = 0.0
            self.twistMsg.linear.z = 0.0
            self.twistIncrement = 0.1
            self.behavior = self.exerciseDT
            rospy.on_shutdown(self.stopDT)
        elif test == "arm":
            self.stowArm = StowArm()
            self.storeObject = StoreObject()
            self.flag = False
            self.gripperService = rospy.ServiceProxy('/abby_gripper/gripper', gripper)
            self.behavior = self.exerciseArm
        self.batteryListener = rospy.Subscriber("power_state", PowerState, self.batteryCallback)
    
    def idle(self):
         pass
        
    def exerciseDT(self):
        self.twistMsg.angular.z = self.twistMsg.angular.z + self.twistIncrement
        if math.fabs(self.twistMsg.angular.z) > 1.57:
        	self.twistIncrement = - self.twistIncrement
        self.commandPublisher.publish(self.twistMsg)
        rospy.sleep(.05)
        
    def stopDT(self):
        self.twistMsg.angular.x = 0.0
        self.twistMsg.angular.y = 0.0
        self.twistMsg.angular.z = 0.0
        self.twistMsg.linear.x = 0.0
        self.twistMsg.linear.y = 0.0
        self.twistMsg.linear.z = 0.0
        self.commandPublisher.publish(self.twistMsg)
    
    def exerciseArm(self):
        if self.flag:
            self.storeObject.sendUntilSuccess()
            self.gripperService(gripperRequest.OPEN)
        else:
            self.stowArm.sendUntilSuccess()
            self.gripperService(gripperRequest.CLOSE)
        self.flag = not self.flag
    
    def run(self):
        while not rospy.is_shutdown():
            self.behavior()
    
    def batteryCallback(self, powerStateMsg):
        rospy.loginfo('Battery Voltage: %fV    13.8v Rail Voltage: %fV    cRIO Voltage: %fV', 
                      powerStateMsg.battery_voltage, 
                      powerStateMsg.v13_8_voltage, 
                      powerStateMsg.cRIO_voltage)
        if self.file_output:
            self.file_handle.write("{0},{1},{2},{3}\n".format( 
                      powerStateMsg.header.stamp, 
                      powerStateMsg.battery_voltage, 
                      powerStateMsg.v13_8_voltage, 
                      powerStateMsg.cRIO_voltage))
        if powerStateMsg.battery_voltage < 21.5:
            rospy.signal_shutdown("Test is complete. Battery at critical voltage! Recharge now!")

if __name__ == '__main__':
    rospy.init_node('battery_analyzer')
    analyzer = BatteryAnalyzer("arm","file")
    analyzer.run()
