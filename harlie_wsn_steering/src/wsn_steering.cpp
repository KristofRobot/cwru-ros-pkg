#include <ros/ros.h>
#include <nav_msgs/Odometry.h>
#include <geometry_msgs/Twist.h>
#include <tf/transform_datatypes.h>
#include <cmath>
#include <harlie_wsn_steering/DesiredState.h>

class WSNSteering {
	public:
		WSNSteering();
	private:
		//callback to put odometry information into the class 
		void odomCallback(const nav_msgs::Odometry::ConstPtr& odom);	
		void desStateCallback(const harlie_wsn_steering::DesiredState::ConstPtr& desState);
		/*The Wyatt Newman JAUSy Steering algorithm
		 * x,y in meters in ROS frame
		 * psi in rads in ROS frame, 0 points to true north
		 * v in meters/sec forwards velocity
		 * omega in rads/sec
		 */
		void computeVelocities(double x_PSO, double y_PSO, double psi_PSO, double x_des, double y_des, double v_des, double psi_des, double rho_des, double &v, double &omega);

		//last updated Odometry information
		nav_msgs::Odometry current_odom;
		harlie_wsn_steering::DesiredState curDesState;

		//Loop rate in Hz
		double loop_rate;

		//put gains here in whatever type you need (int, double, etc) (though more descriptive names than k would make me happier)
		double k_psi;
		double k_v;
		double k_d;
		bool firstCall;
		double x_init,y_init,psi_init;

		//ROS communcators
		ros::NodeHandle nh_;
		ros::NodeHandle priv_nh_;
		ros::Subscriber odom_sub_;
		ros::Subscriber desState_sub_;
		ros::Publisher twist_pub_;

};

const double pi(acos(-1.0));

WSNSteering::WSNSteering() : priv_nh_("~") {
	//Read parameters from the ROS parameter server, defaulting to value if the parameter is not there
	double convergence_rate; //convergence rate in meters
	priv_nh_.param("convergence_rate", convergence_rate, 2.0); 
	priv_nh_.param("k_v", k_v, 1.0);
	priv_nh_.param("loop_rate", loop_rate, 20.0);

	//Computing gains based on convergence_rate parameter
	k_d = 1.0/pow(convergence_rate, 2.0);
	k_psi = 2.0/convergence_rate;

	firstCall=true;
	//Subscribe to Odometry Topic
	odom_sub_ = nh_.subscribe<nav_msgs::Odometry>("odometry", 10, &WSNSteering::odomCallback, this); 
	desState_sub_ = nh_.subscribe<harlie_wsn_steering::DesiredState>("idealState", 10, &WSNSteering::desStateCallback, this);

	//Setup velocity publisher
	twist_pub_ = nh_.advertise<geometry_msgs::Twist>("cmd_vel", 1); 

	//Setup the rate limiter
	ros::Rate rate(loop_rate);

	//temps
	geometry_msgs::Twist twist;
	double v;
	double omega;
	double x_Des,y_Des,v_Des,psi_Des,rho_Des; // need to get these values from trajectory generator;
	double x_PSO,y_PSO,psi_PSO;

	//x_Des = 52.846; // set these via ideal-state generator
	//y_Des = -9.899;
	//psi_Des = -0.7106; // 
	//rho_Des=0; // zero curvature

	//Don't shutdown till the node shuts down
	while(ros::ok()) {
		if (!firstCall) // do this only when PSO is warmed up
		{
			x_Des = curDesState.x;
			y_Des = curDesState.y;
			v_Des = curDesState.v;
			psi_Des = curDesState.theta;
			rho_Des = curDesState.rho;

			//Orientation is a quaternion, so need to get yaw angle in rads.. unless you want a quaternion
			x_PSO = current_odom.pose.pose.position.x;
			y_PSO = current_odom.pose.pose.position.y;
			psi_PSO = tf::getYaw(current_odom.pose.pose.orientation);
			// spoof these: starting point from pit:
			computeVelocities(x_PSO,y_PSO,psi_PSO,x_Des,y_Des,v_Des,psi_Des,rho_Des,v,omega);   

			//Put values into twist message
			twist.linear.x = v;
			twist.angular.z = omega;

			//Publish twist message
			twist_pub_.publish(twist);
		}
		//Make sure this node's ROS stuff gets to run if we are hogging CPU
		ros::spinOnce();

		//Sleep till it's time to go again
		rate.sleep();
	}
}

void WSNSteering::computeVelocities(double x_PSO, double y_PSO, double psi_PSO, double x_des, double y_des, double v_des, double psi_des, double rho_des, double &v, double &omega) {
	//Wyatt put your code here. We will figure out the interface to Beom's GPS points later
	double tanVec[2],nVec[2],dx_vec[2],d;
	double deltaPsi;

	tanVec[0]= cos(psi_des); //-sin(psi_des); // vector tangent to desired lineseg
	tanVec[1]= sin(psi_des); //cos(psi_des); 

	nVec[0]= -tanVec[1];  // normal vector of desired (directed) lineseg--points "left" of heading
	nVec[1]=  tanVec[0];
	dx_vec[0] = x_des-x_PSO;
	dx_vec[1] = y_des-y_PSO; //position error
	double Lfollow = tanVec[0]*dx_vec[0]+tanVec[1]*dx_vec[1];
	v = v_des + k_v * Lfollow;
	// d = -n'*dx_vec;
	d = -nVec[0]*dx_vec[0]-nVec[1]*dx_vec[1];
	deltaPsi = psi_PSO-psi_des;
	while (deltaPsi>pi)
		deltaPsi-=2*pi;
	while (deltaPsi< -pi)
		deltaPsi+=2*pi;
	double rho_cmd = -k_d*d -k_psi*deltaPsi + rho_des;
	omega = v*rho_cmd;
}	

void WSNSteering::odomCallback(const nav_msgs::Odometry::ConstPtr& odom) {
	current_odom = *odom;
	if (firstCall)
	{
		firstCall=false;
		x_init=  current_odom.pose.pose.position.x;
		y_init = current_odom.pose.pose.position.y;
		psi_init = tf::getYaw(current_odom.pose.pose.orientation);
	}
}

void WSNSteering::desStateCallback(const harlie_wsn_steering::DesiredState::ConstPtr& desState)
{
	curDesState= *desState;

}

int main(int argc, char *argv[]) {
	ros::init(argc, argv, "wsn_steering");
	WSNSteering steering;
}
