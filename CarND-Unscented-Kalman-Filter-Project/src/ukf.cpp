#include "ukf.h"
#include "tools.h"
#include "Eigen/Dense"
#include <iostream>

using namespace std;
using Eigen::MatrixXd;
using Eigen::VectorXd;
using std::vector;

/**
 * Initializes Unscented Kalman filter
 */
UKF::UKF()
{
  // if this is false, laser measurements will be ignored (except during init)
  use_laser_ = true;

  // if this is false, radar measurements will be ignored (except during init)
  use_radar_ = true;

  // initial state vector
  x_ = VectorXd(5);

  // initial covariance matrix
  P_ = MatrixXd(5, 5);

  // Process noise standard deviation longitudinal acceleration in m/s^2
  std_a_ = 1.;  //  => Dominik mentions using sigma^2 = 9, original: 30;

  // Process noise standard deviation yaw acceleration in rad/s^2
  std_yawdd_ = 1.;  // original: 30;

  // Laser measurement noise standard deviation position1 in m
  std_laspx_ = 0.15;

  // Laser measurement noise standard deviation position2 in m
  std_laspy_ = 0.15;

  // Radar measurement noise standard deviation radius in m
  std_radr_ = 0.3;

  // Radar measurement noise standard deviation angle in rad
  std_radphi_ = 0.03;

  // Radar measurement noise standard deviation radius change in m/s
  std_radrd_ = 0.3;

  /**
  TODO:

  Complete the initialization. See ukf.h for other member properties.

  Hint: one or more values initialized above might be wildly off...
  */

  is_initialized_ = false;

  time_us_ = 0;

  // state dimension
  n_x_ = 5;

   // augmented state dimension
  n_aug_ = 7;


  // spreading parameter
  lambda_ = 3 - n_x_;

  // set vector for weights
  weights_ = VectorXd(2 * n_aug_ + 1);
  weights_(0) = lambda_ / (lambda_ + n_aug_);

  for (int i = 1; i < 2 * n_aug_ + 1; i++)
  {
	  weights_(i) = 1 / (2 * (lambda_ + n_aug_));
  }
}

UKF::~UKF() {}

/**
 * @param {MeasurementPackage} meas_package The latest measurement data of
 * either radar or laser.
 */
void UKF::ProcessMeasurement(MeasurementPackage meas_package) 
{
  /**
  TODO:

  Complete this function! Make sure you switch between lidar and radar
  measurements.
  */

	//////////////////////////////////////////////////////////////////////////
	//  Initialization step
	//////////////////////////////////////////////////////////////////////////

	if (!is_initialized_) 
	{
		// Initialize the state ekf_.x_
		std::cout << "UKF: " << endl;
		x_ << 1, 1, 1, 1, 1;

		// initialize prediction covariance matrix
		P_ << 1, 0, 0, 0, 0,
			  0, 1, 0, 0, 0,
			  0, 0, 1, 0, 0,
			  0, 0, 0, 1, 0,
			  0, 0, 0, 0, 1;

		if (meas_package.sensor_type_ == MeasurementPackage::RADAR) 
		{
			// convert recieved measurement to unscented kalman initial state
			// set state ekf_.x_ to the first measurement
			double rho = meas_package.raw_measurements_[0];
			double psi = meas_package.raw_measurements_[1];
			double rho_dot = meas_package.raw_measurements_[2];

			double px = rho * cos(psi);
			double py = rho * sin(psi);
			double vx = rho_dot * cos(psi);
			double vy = rho_dot * sin(psi);

			double psi_dot = atan2(vy, vx);

			// initial values are zero:
			if (px == 0 && py == 0)
			{
				px = py = 0.001;
			}

			x_ << px, py, 0, 0, 0;  // x_ << px, py, sqrt(px*px + py*py), psi, psi_dot;
		}
		else if (meas_package.sensor_type_ == MeasurementPackage::LASER) 
		{
			// set state ekf_.x_ to the first measurement.
			double px = meas_package.raw_measurements_[0];
			double py = meas_package.raw_measurements_[1];

			// initial values are zero:
			if (px == 0 && py == 0)
			{
				px = py = 0.001;
			}

			x_ << px, py, 0, 0, 0;
		}

		time_us_ = meas_package.timestamp_;
		is_initialized_ = true;  // done initializing

		return;
	}

	//////////////////////////////////////////////////////////////////////////
	//  Prediction step
	//////////////////////////////////////////////////////////////////////////

	// compute the time elapsed between the current and previous measurements
	double delta_t = (meas_package.timestamp_ - time_us_) / 1000000.0;	//dt - expressed in seconds
	time_us_ = meas_package.timestamp_;

	// make prediction of sigma points
	Prediction(delta_t);

	//////////////////////////////////////////////////////////////////////////
	//  Update step
	//////////////////////////////////////////////////////////////////////////

	if (meas_package.sensor_type_ == MeasurementPackage::RADAR && use_radar_)
	{
		UpdateRadar(meas_package);  // update radar measurement and state estimation
	}
	else if (meas_package.sensor_type_ == MeasurementPackage::LASER && use_laser_)
	{
		UpdateLidar(meas_package);  // update laser measurement and state estimation
	}
}

/**
 * Predicts sigma points, the state, and the state covariance matrix.
 * @param {double} delta_t the change in time (in seconds) between the last
 * measurement and this one.
 */
void UKF::Prediction(double delta_t) 
{
  /**
  TODO:

  Complete this function! Estimate the object's location. Modify the state
  vector, x_. Predict sigma points, the state, and the state covariance matrix.
  */

	//////////////////////////////////////////////////////////////////////////
	// Generate augmented sigma points
	//////////////////////////////////////////////////////////////////////////

	// augmented mean vector
	VectorXd x_aug = VectorXd(7);

	// augmented state covariance
	MatrixXd P_aug = MatrixXd(7, 7);
	P_aug.setZero();

	// sigma point matrix
	MatrixXd Xsig_aug = MatrixXd(n_aug_, 2 * n_aug_ + 1);

	// Q matrix
	MatrixXd Q(2, 2);
	Q << std_a_ * std_a_, 0, 0, std_yawdd_ * std_yawdd_;

	// augmented mean state
	x_aug.head(5) = x_;
	x_aug(5) = 0;
	x_aug(6) = 0;

	// augmented covariance matrix
	P_aug.topLeftCorner<5, 5>() = P_;
	P_aug.bottomRightCorner<2, 2>() = Q;

	// square root matrix
	MatrixXd A = P_aug.llt().matrixL();

	// augmented sigma points
	// set first column of sigma point matrix
	Xsig_aug.col(0) = x_aug;

	// set remaining sigma points
	for (int i = 0; i < n_aug_; i++)
	{
		Xsig_aug.col(i + 1) = x_aug + sqrt(lambda_ + n_aug_) * A.col(i);
		Xsig_aug.col(i + 1 + n_aug_) = x_aug - sqrt(lambda_ + n_aug_) * A.col(i);
	}
	
	//////////////////////////////////////////////////////////////////////////
	// Augmented Sigma point prediction
	//////////////////////////////////////////////////////////////////////////

	Xsig_pred_ = MatrixXd(n_x_, 2 * n_aug_ + 1);

	// predict sigma points
	for (int i = 0; i < 2 * n_aug_ + 1; i++)
	{
		// extract values for better readability
		double p_x = Xsig_aug(0, i);
		double p_y = Xsig_aug(1, i);
		double v = Xsig_aug(2, i);
		double yaw = Xsig_aug(3, i);
		double yawd = Xsig_aug(4, i);
		double nu_a = Xsig_aug(5, i);
		double nu_yawdd = Xsig_aug(6, i);

		// predicted state values
		double px_p, py_p;

		// avoid division by zero
		if (fabs(yawd) > 0.001) 
		{
			px_p = p_x + v / yawd * (sin(yaw + yawd * delta_t) - sin(yaw));
			py_p = p_y + v / yawd * (cos(yaw) - cos(yaw + yawd * delta_t));
		}
		else 
		{
			px_p = p_x + v * delta_t * cos(yaw);
			py_p = p_y + v * delta_t * sin(yaw);
		}

		double v_p = v;
		double yaw_p = yaw + yawd*delta_t;
		double yawd_p = yawd;

		// add noise
		px_p = px_p + 0.5 * nu_a * delta_t * delta_t * cos(yaw);
		py_p = py_p + 0.5 * nu_a * delta_t * delta_t * sin(yaw);
		v_p = v_p + nu_a * delta_t;
		yaw_p = yaw_p + 0.5 * nu_yawdd * delta_t * delta_t;
		yawd_p = yawd_p + nu_yawdd * delta_t;

		// write predicted sigma point into right column
		Xsig_pred_(0, i) = px_p;
		Xsig_pred_(1, i) = py_p;
		Xsig_pred_(2, i) = v_p;
		Xsig_pred_(3, i) = yaw_p;
		Xsig_pred_(4, i) = yawd_p;
	}
	//////////////////////////////////////////////////////////////////////////

	// predict state mean
	x_.fill(0.0);
	for (int i = 0; i < 2 * n_aug_ + 1; i++)  // iterate over sigma points
	{  
		x_ = x_ + weights_(i) * Xsig_pred_.col(i);
	}

	// predicted state covariance matrix
	P_.fill(0.0);
	for (int i = 0; i < 2 * n_aug_ + 1; i++)  // iterate over sigma points
	{  
		// state difference
		VectorXd x_diff = Xsig_pred_.col(i) - x_;

		// angle normalization
		x_diff(3) = tools.NormalizeAng(x_diff(3));

		P_ = P_ + weights_(i) * x_diff * x_diff.transpose();
	}
}

/**
 * Updates the state and the state covariance matrix using a laser measurement.
 * @param {MeasurementPackage} meas_package
 */
void UKF::UpdateLidar(MeasurementPackage meas_package) 
{
  /**
  TODO:

  Complete this function! Use lidar data to update the belief about the object's
  position. Modify the state vector, x_, and covariance, P_.

  You'll also need to calculate the lidar NIS.
  */

	// sensor state dimension
	int n_z = 2;

	// create matrix for sigma points in measurement space
	MatrixXd Zsig = MatrixXd(n_z, 2 * n_aug_ + 1);

	//////////////////////////////////////////////////////////////////////////
	// transform sigma points into measurement space
	//////////////////////////////////////////////////////////////////////////

	for (int i = 0; i < 2 * n_aug_ + 1; i++)  //2n+1 simga points
	{  

		// extract values for better readibility
		double p_x = Xsig_pred_(0, i);
		double p_y = Xsig_pred_(1, i);
		double v = Xsig_pred_(2, i);
		double yaw = Xsig_pred_(3, i);

		// measurement model
		Zsig(0, i) = Xsig_pred_(0, i);  //px
		Zsig(1, i) = Xsig_pred_(1, i);  //py
	}

	// mean predicted measurement
	VectorXd z_pred = VectorXd(n_z);
	z_pred.fill(0.0);

	for (int i = 0; i < 2 * n_aug_ + 1; i++) 
	{
		z_pred = z_pred + weights_(i) * Zsig.col(i);
	}

	// measurement covariance matrix S
	MatrixXd S = MatrixXd(n_z, n_z);
	S.fill(0.0);

	for (int i = 0; i < 2 * n_aug_ + 1; i++)  // 2n+1 sigma points
	{  
		VectorXd z_diff = Zsig.col(i) - z_pred;  // residual

		// angle normalization
		z_diff(1) = tools.NormalizeAng(z_diff(1));

		S = S + weights_(i) * z_diff * z_diff.transpose();
	}

	// add measurement noise covariance matrix
	MatrixXd R = MatrixXd(n_z, n_z);

	R << std_laspx_ * std_laspx_, 0, 0, std_laspy_ * std_laspy_;
	S = S + R;

	//////////////////////////////////////////////////////////////////////////

	//create matrix for cross correlation Tc
	MatrixXd Tc = MatrixXd(n_x_, n_z);
	Tc.fill(0.0);

	// calculate cross correlation matrix
	for (int i = 0; i < 2 * n_aug_ + 1; i++)  // 2n+1 sigma points
	{  
		VectorXd z_diff = Zsig.col(i) - z_pred;  // residual

		// angle normalization
		z_diff(1) = tools.NormalizeAng(z_diff(1));

		// state difference
		VectorXd x_diff = Xsig_pred_.col(i) - x_;

		// angle normalization
		x_diff(3) = tools.NormalizeAng(x_diff(3));

		// cross-correlation matrix
		Tc = Tc + weights_(i) * x_diff * z_diff.transpose();
	}

	// Kalman gain K;
	MatrixXd K = Tc * S.inverse();

	// actual measurement
	VectorXd z = VectorXd(n_z);

	z << meas_package.raw_measurements_[0], meas_package.raw_measurements_[1];

	// residual
	VectorXd z_diff = z - z_pred;

	// angle normalization
	z_diff(1) = tools.NormalizeAng(z_diff(1));

	// update state mean and covariance matrix
	x_ = x_ + K * z_diff;
	P_ = P_ - K * S * K.transpose();

	// calculate the lidar NIS.
	NIS_laser_ = z_diff.transpose() * S.inverse() * z_diff;
	//std::cout << "NIS error= \n" << NIS_laser_ << std::endl;

	std::cout << "x = \n" << x_ << std::endl;
	std::cout << "P = \n" << P_ << std::endl;
}

/**
 * Updates the state and the state covariance matrix using a radar measurement.
 * @param {MeasurementPackage} meas_package
 */
void UKF::UpdateRadar(MeasurementPackage meas_package) 
{
  /**
  TODO:

  Complete this function! Use radar data to update the belief about the object's
  position. Modify the state vector, x_, and covariance, P_.

  You'll also need to calculate the radar NIS.
  */

	// sensor state dimension
	int n_z = 3;

	// create matrix for sigma points in measurement space
	MatrixXd Zsig = MatrixXd(n_z, 2 * n_aug_ + 1);

	//////////////////////////////////////////////////////////////////////////
	// transform sigma points into measurement space
	//////////////////////////////////////////////////////////////////////////

	for (int i = 0; i < 2 * n_aug_ + 1; i++)  //2n+1 simga points
	{

		// extract values for better readibility
		double p_x = Xsig_pred_(0, i);
		double p_y = Xsig_pred_(1, i);
		double v = Xsig_pred_(2, i);
		double yaw = Xsig_pred_(3, i);

		double v1 = cos(yaw)*v;
		double v2 = sin(yaw)*v;

		// measurement model
		Zsig(0, i) = sqrt(p_x * p_x + p_y * p_y);							// r
		Zsig(1, i) = atan2(p_y, p_x);										// phi
		Zsig(2, i) = (p_x * v1 + p_y * v2) / sqrt(p_x * p_x + p_y * p_y);   // r_dot
	}

	// mean predicted measurement
	VectorXd z_pred = VectorXd(n_z);
	z_pred.fill(0.0);

	for (int i = 0; i < 2 * n_aug_ + 1; i++) 
	{
		z_pred = z_pred + weights_(i) * Zsig.col(i);
	}

	// measurement covariance matrix S
	MatrixXd S = MatrixXd(n_z, n_z);
	S.fill(0.0);

	for (int i = 0; i < 2 * n_aug_ + 1; i++)  //2n+1 simga points
	{
		// residual
		VectorXd z_diff = Zsig.col(i) - z_pred;

		//angle normalization
		z_diff(1) = tools.NormalizeAng(z_diff(1));

		S = S + weights_(i) * z_diff * z_diff.transpose();
	}

	//add measurement noise covariance matrix
	MatrixXd R = MatrixXd(n_z, n_z);
	R << std_radr_ * std_radr_, 0, 0,	0, std_radphi_ * std_radphi_, 0, 0, 0, std_radrd_ * std_radrd_;
	S = S + R;

	//////////////////////////////////////////////////////////////////////////

	// create matrix for cross correlation Tc
	MatrixXd Tc = MatrixXd(n_x_, n_z);
	Tc.fill(0.0);

	//calculate cross correlation matrix
	for (int i = 0; i < 2 * n_aug_ + 1; i++)  //2n+1 simga points
	{
		// residual
		VectorXd z_diff = Zsig.col(i) - z_pred;

		// angle normalization
		z_diff(1) = tools.NormalizeAng(z_diff(1));

		// state difference
		VectorXd x_diff = Xsig_pred_.col(i) - x_;

		// angle normalization
		x_diff(3) = tools.NormalizeAng(x_diff(3));

		Tc = Tc + weights_(i) * x_diff * z_diff.transpose();
	}

	// Kalman gain K;
	MatrixXd K = Tc * S.inverse();

	// actual measurement
	VectorXd z = VectorXd(n_z);
	z << meas_package.raw_measurements_[0], meas_package.raw_measurements_[1], meas_package.raw_measurements_[2];

	// residual
	VectorXd z_diff = z - z_pred;

	// angle normalization
	z_diff(1) = tools.NormalizeAng(z_diff(1));

	// update state mean and covariance matrix
	x_ = x_ + K * z_diff;
	P_ = P_ - K * S * K.transpose();

	NIS_radar_ = z_diff.transpose() * S.inverse() * z_diff;
	//std::cout << "NIS error= \n" << NIS_laser_ << std::endl;

	std::cout << "x = \n" << x_ << std::endl;
	std::cout << "P = \n" << P_ << std::endl;
}