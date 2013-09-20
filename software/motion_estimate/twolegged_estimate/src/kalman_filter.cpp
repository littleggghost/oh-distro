
#include <leg-odometry/kalman_filter.hpp>
#include <iostream>

KalmanFilter::KalmanFilter() {

}


KalmanFilter::KalmanFilter(KalmanFilter_Models::BaseModel &def_model) {
	std::cout << "KalmanFilter::KalmanFilter(KalmanFilter_Models::BaseModel -- saying hi." << std::endl;

	// TODO -- Add more inheretance KF models here
	// Simpler, but no simpler
	// This will have to be done for each new model we define :-/ but avoids difficulty in variation of parameters with callback approach
	if (_model = dynamic_cast<KalmanFilter_Models::Joint_Model*>(&def_model) ) {
		// we have a joint filter
		std::cout << "KalmanFilter::KalmanFilter -- Copying pointer for Joint filter" << std::endl;
	} else if (_model = dynamic_cast<KalmanFilter_Models::DataFusion_Model*>(&def_model)) {
		// we have a data fusion filter
		std::cout << "KalmanFilter::KalmanFilter -- Copying pointer for DataFusion filter" << std::endl;
	}
}

KalmanFilter::~KalmanFilter() {
	//free()
	
	
}


void KalmanFilter::Initialize() {
	std::cout << "KalmanFilter::Initialize -- Initializing." << std::endl;
	_model->identify();
	
	ut_last_priori_update = 0;
	// we need to setup the different matrices to have the correct size
	_model->setSizes(priori);
	std::cout << "KalmanFilter::Initialize -- priori.mu resized to: " << priori.mu.rows() << std::endl;
	
	return;
}

void KalmanFilter::step(const unsigned long &ut_now, const VAR_VECTORd &variables) {
	
	std::cout << "KalmanFilter::step -- Time stepping the filter." << std::endl;
	
	//priori update
	propagatePriori(ut_now, variables, priori.mu, priori.M);
	
	return; // return the results of the filtering process here
}


void KalmanFilter::step(const unsigned long &ut_now, const VAR_VECTORd &variables, const VAR_VECTORd &measurements) {
	
	step(ut_now, variables);
	std::cout << "KalmanFilter::step -- Measurement stepping the filter." << std::endl;
	//std::cout << "KalmanFilter::step -- measurements(0): " << measurements(0) << std::endl;
	
	//priori
	//propagatePriori(ut_now);
	
	//posterior
	
	return; // return the results of the filtering process here
}


KalmanFilter_Types::Priori KalmanFilter::propagatePriori(const unsigned long &ut_now, const VAR_VECTORd &variables, const VAR_VECTORd &mu, const VAR_MATRIXd &cov) {
	
	std::cout << "KalmanFilter::propagatePriori -- Time update" << std::endl;
	last_update_type = PRIORI_UPDATE;
	
	double dt;
	
	dt = 1E-6*(ut_now - ut_last_priori_update);
	ut_last_priori_update = ut_now;
	std::cout << "KalmanFilter::propagatePriori -- dt = " << dt << std::endl;
	
	// We want to propagate a current state mean and covariance estimate according to the some defined system model
	
	
	// Get continuous dynamics model -> convert to discrete -> propagate state mean and covariance
	KalmanFilter_Models::MatricesUnit cont_matrices, disc_matrices; // why am I keeping a local copy here?
	cont_matrices = _model->getContinuousMatrices(mu);

	// s -> z, lti_disc
	disc_matrices = lti_disc(dt, cont_matrices);
	
	// propagate mean (mu)
	if (_model->getSettings().propagate_with_linearized == true) {
		// Propagate the state with transition matrix
		
		priori.mu.resize(mu.size());
//		std::cout << "KalmanFilter::propagatePriori -- mu.size() -- " << mu.size() << std::endl;
		priori.mu = disc_matrices.A * mu;// TODO -- add B*u term, we going to assume this is noise for now
		
	} else {
		std::cerr << "KalmanFilter::propagatePriori -- oops, non-linear propagation not implemented!!" << std::endl;
		std::cout << "KalmanFilter::propagatePriori -- oops, non-linear propagation not implemented!!" << std::endl;
				
		// Propagate with non-linear model
		// this should include an integration period and some good integration method
		//priori.mu = _model->propagation_model(post.mu);
	}
	
	// Prepare process covariance matrix
	
	
	
	// Compute dynamics matrix
	// Compute state transition and discrete process covariance matrices
	
	// Compute priori covariance matrix
	
	// Compute Kalman Gain matrix
	
	
	return priori;
}


KalmanFilter_Types::Posterior KalmanFilter::propagatePosterior() {
	
	std::cout << "Measurement update" << std::endl;
	
	last_update_type = POSTERIOR_UPDATE;
	
	KalmanFilter_Types::Posterior temp;
	
	
	return temp;
}



KalmanFilter_Models::MatricesUnit KalmanFilter::lti_disc(const double &dt, const KalmanFilter_Models::MatricesUnit &cont) {
	
	
	// Compute discrete process noise covariance
	KalmanFilter_Models::MatricesUnit disc;

	//n   = size(F,1);
	//Phi = [F L*Q*L'; zeros(n,n) -F'];
	//AB  = expm(Phi*dt)*[zeros(n,n);eye(n)];
	//Qd   = AB(1:n,:)/AB((n+1):(2*n),:)
	
	disc.A = expm(dt, cont.A);
	
	int n = cont.A.rows();
	
	VAR_MATRIXd shaped_Q;
	
	shaped_Q = cont.B * cont.Q * (cont.B.transpose());
	
	// Create and pack the Phi matrix
	VAR_MATRIXd Phi;
	Phi.resize(2*n, 2*n);
	VAR_MATRIXd rhsAB(2*n,2);
	rhsAB.setZero();

	std::cout << "KalmanFilter::lti_disc -- " << n << " " << shaped_Q.rows() << " " << shaped_Q.cols() << std::endl;
	
	int i,j;
	for (i=0;i<n;i++) {
		for (j=0;j<n;j++) {
			// North West
			Phi(i,j) = cont.A(i,j);
			//South West
			Phi(n+i,j) = 0.;
			//North East
			Phi(i,n+j) = shaped_Q(i,j);
			//South East
			Phi(n+i,n+j) = -cont.A(j,i);
		}
		rhsAB(i,i) = 1;
	}
	
	// compute AB
	VAR_MATRIXd AB;
	AB = expm(dt,Phi) * rhsAB;
	
	VAR_MATRIXd over(n,n);
	VAR_MATRIXd under(n,n);
	
	// Compute discrete process noise covariance matrix
	for (i=0;i<n;i++) {
		for (j=0;j<n;j++) {
			//Qd   = AB(1:n,:)/AB((n+1):(2*n),:)
			over(i,j) = AB(i,j);
			under(i,j) = AB(n+1,j);
		}
	}
	
	disc.Q = over*(under.inverse());
	
	return disc;
	
}

// Return a copy of the internal Kalman Filter state estimate and Covariance
// Will return the structure with variables indicating whether the last was a priori of posterior update, including the utime of the last update
KalmanFilter_Types::State KalmanFilter::getState() {
	KalmanFilter_Types::State state;
	
	state.last_update_type = last_update_type;
	
	if (last_update_type == PRIORI_UPDATE) {
		state.utime = priori.utime;
		
		state.X = priori.mu;
		state.Cov = priori.M;
	} else {
		state.utime = posterior.utime;
		
		state.X = posterior.mu;
		state.Cov = posterior.P;
	}
		
	return state;
}

VAR_MATRIXd KalmanFilter::expm(const double &dt, const VAR_MATRIXd &F) {
	
	VAR_MATRIXd Phi;
	VAR_MATRIXd eye;
	eye.setIdentity(F.rows(), F.cols());
	
	return (eye + dt*F + 0.5*dt*dt*F*F);
}


