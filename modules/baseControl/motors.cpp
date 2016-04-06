/*
* Copyright (C)2015  iCub Facility - Istituto Italiano di Tecnologia
* Author: Marco Randazzo
* email:  marco.randazzo@iit.it
* website: www.robotcub.org
* Permission is granted to copy, distribute, and/or modify this program
* under the terms of the GNU General Public License, version 2 or any
* later version published by the Free Software Foundation.
*
* A copy of the license can be found at
* http://www.robotcub.org/icub/license/gpl.txt
*
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
* Public License for more details
*/

#include "motors.h"
#include "filters.h"
#include <yarp/os/Log.h>
#include <yarp/os/LogStream.h>

bool MotorControl::set_control_openloop()
{
    yInfo ("Setting openloop mode");
    icmd->setOpenLoopMode(0);
    icmd->setOpenLoopMode(1);
    iopl->setRefOutput(0,0);
    iopl->setRefOutput(1,0);
    return true;
}

bool MotorControl::set_control_velocity()
{
    yInfo ("Setting velocity mode");
    icmd->setVelocityMode(0);
    icmd->setVelocityMode(1);
    ivel->velocityMove(0,0);
    ivel->velocityMove(1,0);
    return true;
}

bool MotorControl::set_control_idle()
{
    yInfo ("Setting ilde mode");
    icmd->setControlMode(0, VOCAB_CM_IDLE);
    icmd->setControlMode(1, VOCAB_CM_IDLE);
    icmd->setControlMode(2, VOCAB_CM_IDLE);
    icmd->setControlMode(3, VOCAB_CM_IDLE);
    yInfo("Motors now off");
    return true;
}

bool MotorControl::check_motors_on()
{
    int c0(0),c1(0),c2(0);
    yarp::os::Time::delay(0.05);
    icmd->getControlMode(0,&c0);
    icmd->getControlMode(0,&c1);
    if (c0 != VOCAB_CM_IDLE && c1 != VOCAB_CM_IDLE)
    {
        yInfo("Motors now on\n");
        return true;
    }
    else
    {
        yInfo("Unable to turn motors on! fault pressed?\n");
        return false;
    }
}

void MotorControl::updateControlMode()
{
    icmd->getControlModes(board_control_modes);
    /*
        for (int i=0; i<3; i++)
        if (board_control_modes[i]==VOCAB_CM_IDLE)
        {
            yWarning ("One motor is in idle state. Turning off control.");
            turn_off_control();
            break;
        }
    */
}

void MotorControl::printStats()
{
    yInfo( "* Motor thread:\n");
    yInfo( "timeouts: %d joy: %d cmd %d\n", thread_timeout_counter, joy_timeout_counter, mov_timeout_counter);

    if (joystick_received>0) 
        yInfo( "Under joystick control (%d)\n", joystick_received);

    double val = 0;
    for (int i=0; i<2; i++)
    {
        if      (i==0) val=F_L;
        else if (i==1) val=F_R;
        if (board_control_modes[i]==VOCAB_CM_IDLE)
            yInfo( "F%d: IDLE\n", i);
        else
            yInfo( "F%d: %+.1f\n", i, val);
    }
}

void MotorControl::close()
{
    port_movement_control.interrupt();
    port_movement_control.close();
    port_auxiliary_control.interrupt();
    port_auxiliary_control.close();
    port_joystick_control.interrupt();
    port_joystick_control.close();
}

MotorControl::~MotorControl()
{
    close();
}

bool MotorControl::open()
{
    if (rf.check("no_motors_filter"))
    {
        yInfo("'no_filter' option found. Turning off PWM filter.");
        motors_filter_enabled=0;
    }

    // open the interfaces for the control boards
    bool ok = true;
    ok = ok & control_board_driver->view(ivel);
    ok = ok & control_board_driver->view(ienc);
    ok = ok & control_board_driver->view(iopl);
    ok = ok & control_board_driver->view(ipid);
    ok = ok & control_board_driver->view(iamp);
    ok = ok & control_board_driver->view(icmd);
    if(!ok)
    {
        yError("One or more devices has not been viewed\nreturning...\n");
        //return false;
    }
    // open control input ports
    port_movement_control.open((localName+"/control:i").c_str());
    port_auxiliary_control.open((localName+"/aux_control:i").c_str());
    port_joystick_control.open((localName+"/joystick:i").c_str());

    return true;
}

MotorControl::MotorControl(unsigned int _period, ResourceFinder &_rf, Property options,
            PolyDriver* _driver) :
            rf(_rf),
            ctrl_options (options)
    {
    control_board_driver= _driver;

    thread_timeout_counter     = 0;

    command_received    = 0;
    joystick_received   = 0;
    auxiliary_received  = 0;

    mov_timeout_counter = 0;
    joy_timeout_counter = 0;
    aux_timeout_counter = 0;

    F_L = 0;
    F_R = 0;

    joy_linear_speed = 0;
    joy_angular_speed = 0;
    joy_desired_direction = 0;
    joy_pwm_gain = 0;

    cmd_linear_speed = 0;
    cmd_angular_speed = 0;
    cmd_desired_direction = 0;
    cmd_pwm_gain = 0;

    aux_linear_speed = 0;
    aux_angular_speed = 0;
    aux_desired_direction = 0;
    aux_pwm_gain = 0;

    max_linear_vel = DEFAULT_MAX_LINEAR_VEL;
    max_angular_vel = DEFAULT_MAX_ANGULAR_VEL;
    motors_filter_enabled = ctrl_options.findGroup("GENERAL").check("motors_filter_enabled",Value(4),"motors filter frequency (1/2/4/8Hz, 0 = disabled)").asInt();
    
    double tmp =0;
    tmp = (ctrl_options.findGroup("GENERAL").check("max_angular_vel", Value(0), "maximum angular velocity of the platform [deg/s]")).asDouble();
    if (tmp>0 && tmp < DEFAULT_MAX_ANGULAR_VEL) max_angular_vel = tmp;
    tmp = (ctrl_options.findGroup("GENERAL").check("max_linear_vel", Value(0), "maximum linear velocity of the platform [m/s]")).asDouble();
    if (tmp>0 && tmp < DEFAULT_MAX_LINEAR_VEL) max_linear_vel = tmp;
    
    thread_period = _period;
    localName = ctrl_options.find("local").asString();
}

void MotorControl::read_percent_polar(const Bottle *b, double& des_dir, double& lin_spd, double& ang_spd, double& pwm_gain)
{
    des_dir  = b->get(1).asDouble();
    lin_spd  = b->get(2).asDouble();
    ang_spd  = b->get(3).asDouble();
    pwm_gain = b->get(4).asDouble();
    lin_spd  = (lin_spd<+100)  ? lin_spd  : +100; 
    lin_spd  = (lin_spd>-100)  ? lin_spd  : -100;
    ang_spd  = (ang_spd<+100)  ? ang_spd  : +100; 
    ang_spd  = (ang_spd>-100)  ? ang_spd  : -100;
    pwm_gain = (pwm_gain<+100) ? pwm_gain : +100; 
    pwm_gain = (pwm_gain>0)    ? pwm_gain : 0;
}

void MotorControl::read_percent_cart(const Bottle *b, double& des_dir, double& lin_spd, double& ang_spd, double& pwm_gain)
{
    double x_speed        = b->get(1).asDouble();
    double y_speed        = b->get(2).asDouble();
    double t_speed        = b->get(3).asDouble();
    pwm_gain = b->get(4).asDouble();
    des_dir  = atan2(x_speed,y_speed) * 180.0 / 3.14159265;
    lin_spd  = sqrt (x_speed*x_speed+y_speed*y_speed);
    ang_spd  = t_speed;
    lin_spd  = (lin_spd<+100)  ? lin_spd  : +100; 
    lin_spd  = (lin_spd>-100)  ? lin_spd  : -100;
    ang_spd  = (ang_spd<+100)  ? ang_spd  : +100; 
    ang_spd  = (ang_spd>-100)  ? ang_spd  : -100;
    pwm_gain = (pwm_gain<+100) ? pwm_gain : +100; 
    pwm_gain = (pwm_gain>0)    ? pwm_gain : 0;
}

void MotorControl::read_speed_polar(const Bottle *b, double& des_dir, double& lin_spd, double& ang_spd, double& pwm_gain)
{
    des_dir  = b->get(1).asDouble();
    lin_spd  = b->get(2).asDouble() * 100.0 / max_linear_vel;
    ang_spd  = b->get(3).asDouble() * 100.0 / max_angular_vel;
    lin_spd  = (lin_spd<+100)  ? lin_spd  : +100; 
    lin_spd  = (lin_spd>-100)  ? lin_spd  : -100;
    ang_spd  = (ang_spd<+100)  ? ang_spd  : +100; 
    ang_spd  = (ang_spd>-100)  ? ang_spd  : -100;
    pwm_gain = 100;
}

void MotorControl::read_speed_cart(const Bottle *b, double& des_dir, double& lin_spd, double& ang_spd, double& pwm_gain)
{
    double x_speed        = b->get(1).asDouble() * 100.0 / max_linear_vel;
    double y_speed        = b->get(2).asDouble() * 100.0 / max_linear_vel;
    double t_speed        = b->get(3).asDouble() * 100.0 / max_angular_vel;
    des_dir  = atan2(x_speed,y_speed) * 180.0 / 3.14159265;
    lin_spd  = sqrt (x_speed*x_speed+y_speed*y_speed);
    ang_spd  = t_speed;
    lin_spd  = (lin_spd<+100) ? lin_spd : +100; 
    lin_spd  = (lin_spd>-100) ? lin_spd : -100;
    ang_spd  = (ang_spd<+100) ? ang_spd : +100; 
    ang_spd  = (ang_spd>-100) ? ang_spd : -100;
    pwm_gain = 100;
}

void MotorControl::read_inputs(double *linear_speed,double *angular_speed,double *desired_direction, double *pwm_gain)
{
    static double wdt_old_mov_cmd=Time::now();
    static double wdt_old_joy_cmd=Time::now();
    static double wdt_old_aux_cmd=Time::now();
    static double wdt_mov_cmd=Time::now();
    static double wdt_joy_cmd=Time::now();
    static double wdt_aux_cmd=Time::now();

    if (Bottle *b = port_joystick_control.read(false))
    {                
        if (b->get(0).asInt()==1)
        {                                
            //received a joystick command.
            read_percent_polar(b, joy_desired_direction,joy_linear_speed,joy_angular_speed,joy_pwm_gain);
            wdt_old_joy_cmd = wdt_joy_cmd;
            wdt_joy_cmd = Time::now();

            //Joystick commands have higher priorty respect to movement commands.
            //this make the joystick to take control for 100*20 ms
            if (joy_pwm_gain>10) joystick_received = 100;
        }
    }
    if (Bottle *b = port_movement_control.read(false))
    {
        if (b->get(0).asInt()==1)
        {
            read_percent_polar(b, cmd_desired_direction,cmd_linear_speed,cmd_angular_speed,cmd_pwm_gain);
            wdt_old_mov_cmd = wdt_mov_cmd;
            wdt_mov_cmd = Time::now();
            command_received = 100;
        }
        else if (b->get(0).asInt()==2)
        {
            read_speed_polar(b, cmd_desired_direction,cmd_linear_speed,cmd_angular_speed,cmd_pwm_gain);
            wdt_old_mov_cmd = wdt_mov_cmd;
            wdt_mov_cmd = Time::now();
            command_received = 100;
        }
        else if (b->get(0).asInt()==3)
        {
            read_speed_cart(b, cmd_desired_direction,cmd_linear_speed,cmd_angular_speed,cmd_pwm_gain);
            wdt_old_mov_cmd = wdt_mov_cmd;
            wdt_mov_cmd = Time::now();
            command_received = 100;
        }
    }
    if (Bottle *b = port_auxiliary_control.read(false))
    {
        if (b->get(0).asInt()==1)
        {
            read_percent_polar(b, aux_desired_direction,aux_linear_speed,aux_angular_speed,aux_pwm_gain);
            wdt_old_aux_cmd = wdt_aux_cmd;
            wdt_aux_cmd = Time::now();
            auxiliary_received = 100;
        }
        else if (b->get(0).asInt()==2)
        {
            read_speed_polar(b, aux_desired_direction,aux_linear_speed,aux_angular_speed,aux_pwm_gain);
            wdt_old_aux_cmd = wdt_aux_cmd;
            wdt_aux_cmd = Time::now();
            auxiliary_received = 100;
        }
        else if (b->get(0).asInt()==3)
        {
            read_speed_cart(b, aux_desired_direction,aux_linear_speed,aux_angular_speed,aux_pwm_gain);
            wdt_old_aux_cmd = wdt_aux_cmd;
            wdt_aux_cmd = Time::now();
            auxiliary_received = 100;
        }
    }

    //priority test 
    if (joystick_received>0)
    {
        *desired_direction  = joy_desired_direction;
        *linear_speed       = joy_linear_speed;
        *angular_speed      = joy_angular_speed;
        *pwm_gain           = joy_pwm_gain;
    }
    else if (auxiliary_received>0)
    {
        *desired_direction  = aux_desired_direction;
        *linear_speed       = aux_linear_speed;
        *angular_speed      = aux_angular_speed;
        *pwm_gain           = aux_pwm_gain;
    }
    else //if (command_received>0)
    {
        *desired_direction  = cmd_desired_direction;
        *linear_speed       = cmd_linear_speed;
        *angular_speed      = cmd_angular_speed;
        *pwm_gain           = cmd_pwm_gain;
    }

    //watchdog on received commands
    static double wdt_old=Time::now();
    double wdt=Time::now();
    //yDebug("period: %f\n", wdt-wdt_old);
    if (wdt-wdt_mov_cmd > 0.200)
    {
        cmd_desired_direction=0;
        cmd_linear_speed=0;
        cmd_angular_speed=0;
        cmd_pwm_gain=0;
        mov_timeout_counter++; 
    }
    if (wdt-wdt_joy_cmd > 0.200)
    {
        joy_desired_direction=0;
        joy_linear_speed=0;
        joy_angular_speed=0;
        joy_pwm_gain=0;
        joy_timeout_counter++;
    }
    if (wdt-wdt_aux_cmd > 0.200)
    {
        aux_desired_direction=0;
        aux_linear_speed=0;
        aux_angular_speed=0;
        aux_pwm_gain=0;
        aux_timeout_counter++;
    }

    if (wdt-wdt_old > 0.040) { thread_timeout_counter++;  }
    wdt_old=wdt;

    if (joystick_received>0)   { joystick_received--; }
    if (command_received>0)    { command_received--; }
    if (auxiliary_received>0)  { auxiliary_received--; }
}

void MotorControl::decouple(double appl_linear_speed, double appl_angular_speed)
{
    //wheel contribution calculation
    F_L = appl_linear_speed + appl_angular_speed;
    F_R = appl_linear_speed - appl_angular_speed;
}

void MotorControl::execute_speed(double appl_linear_speed, double appl_angular_speed)
{
    decouple(appl_linear_speed,appl_angular_speed);

    //Use a low pass filter to obtain smooth control
    if (motors_filter_enabled == 1) 
    {
        F_L = control_filters::lp_filter_1Hz(F_L, 0);
        F_R = control_filters::lp_filter_1Hz(F_R, 1);
    }
    else if (motors_filter_enabled == 2) 
    {
        F_L = control_filters::lp_filter_2Hz(F_L, 0);
        F_R = control_filters::lp_filter_2Hz(F_R, 1);
    }
    else if (motors_filter_enabled == 4) //default
    {
        F_L = control_filters::lp_filter_4Hz(F_L, 0);
        F_R = control_filters::lp_filter_4Hz(F_R, 1);
    }
    else if (motors_filter_enabled == 8)
    {
        F_L = control_filters::lp_filter_8Hz(F_L, 0);
        F_R = control_filters::lp_filter_8Hz(F_R, 1);
    }

    //Apply the commands
#ifdef  CONTROL_DEBUG
    yDebug (">**: %+6.6f %+6.6f **** %+6.6f %+6.6f\n",exec_linear_speed,exec_desired_direction,-F_L,-F_R);
#endif
    ivel->velocityMove(0,-F_L);
    ivel->velocityMove(1,-F_R);
}

void MotorControl::execute_openloop(double appl_linear_speed, double appl_angular_speed)
{
    decouple(appl_linear_speed,appl_angular_speed);

    //Use a low pass filter to obtain smooth control
    if (motors_filter_enabled>0)
    {
        F_L = control_filters::lp_filter_4Hz(F_L, 0);
        F_R = control_filters::lp_filter_4Hz(F_R, 1);
        //F_L  = ratelim_filter_0(F_L,0);
        //F_R  = ratelim_filter_0(F_R,1);
    }

    //Apply the commands
    iopl->setRefOutput(0, -F_L);
    iopl->setRefOutput(1, -F_R);
}

void MotorControl::execute_none()
{
    iopl->setRefOutput(0,0);
    iopl->setRefOutput(1,0);
    iopl->setRefOutput(2,0);
}