/**
 * particle_filter.cpp
 *
 * Created on: Dec 12, 2016
 * Author: Tiffany Huang
 */

#include "particle_filter.h"

#include <math.h>
#include <algorithm>
#include <iostream>
#include <iterator>
#include <numeric>
#include <random>
#include <string>
#include <vector>

#include "helper_functions.h"

using std::string;
using std::vector;

void ParticleFilter::init(double x, double y, double theta, double std[]) {
  /**
   * TODO: Set the number of particles. Initialize all particles to 
   *   first position (based on estimates of x, y, theta and their uncertainties
   *   from GPS) and all weights to 1. 
   * TODO: Add random Gaussian noise to each particle.
   * NOTE: Consult particle_filter.h for more information about this method 
   *   (and others in this file).
   */
  num_particles = 100;  // TODO: Set the number of particles

  std::default_random_engine generator;
  std::normal_distribution<double> dist_x(x, std[0]);
  std::normal_distribution<double> dist_y(y, std[1]);
  std::normal_distribution<double> dist_theta(theta, std[2]);

  for (int i = 0; i < num_particles; ++i)
  {
    // initialize the particle
    Particle particle;
    particle.id = i;
    particle.weight = 1;
    particle.x = dist_x(generator);
    particle.y = dist_y(generator);
    particle.theta = dist_theta(generator);

    particles.push_back(particle);
  }

  is_initialized = true;
}

void ParticleFilter::prediction(double delta_t, double std_pos[], 
                                double velocity, double yaw_rate) {
  /**
   * TODO: Add measurements to each particle and add random Gaussian noise.
   * NOTE: When adding noise you may find std::normal_distribution 
   *   and std::default_random_engine useful.
   *  http://en.cppreference.com/w/cpp/numeric/random/normal_distribution
   *  http://www.cplusplus.com/reference/random/default_random_engine/
   */
  num_particles = particles.size();

  std::default_random_engine generator;

  for (auto& particle : particles)
  {
    double measurement_x, measurement_y, measurement_theta;
    
    if (yaw_rate < 1e-6)
    {
      measurement_x = particle.x + velocity * cos(particle.theta);
      measurement_y = particle.y + velocity * sin(particle.theta);
      measurement_theta = particle.theta;
    }
    else
    {
      measurement_x = particle.x + velocity / yaw_rate * (sin(particle.theta + yaw_rate * delta_t) - sin(particle.theta));
      measurement_y = particle.y + velocity / yaw_rate * (cos(particle.theta) - cos(particle.theta + yaw_rate * delta_t));
      measurement_theta = particle.theta + yaw_rate * delta_t;
    }

    std::normal_distribution<double> dist_x(measurement_x, std_pos[0]);
    std::normal_distribution<double> dist_y(measurement_y, std_pos[1]);
    std::normal_distribution<double> dist_theta(measurement_theta, std_pos[2]);

    particle.x = dist_x(generator);
    particle.y = dist_y(generator);
    particle.theta = dist_theta(generator);
  }
}

void ParticleFilter::dataAssociation(vector<LandmarkObs> predicted, 
                                     vector<LandmarkObs>& observations) {
  /**
   * TODO: Find the predicted measurement that is closest to each 
   *   observed measurement and assign the observed measurement to this 
   *   particular landmark.
   * NOTE: this method will NOT be called by the grading code. But you will 
   *   probably find it useful to implement this method and use it as a helper 
   *   during the updateWeights phase.
   */
  for (auto& obs : observations)
  {
    double dist_min = std::numeric_limits<double>::max();
    for (auto& pred : predicted)
    {
      double d = dist(pred.x, pred.y, obs.x, obs.y);
      if (d < dist_min)
      {
        obs.id = pred.id;
        dist_min = d;
      }
    }
  }
}

void ParticleFilter::updateWeights(double sensor_range, double std_landmark[], 
                                   const vector<LandmarkObs> &observations, 
                                   const Map &map_landmarks) {
  /**
   * TODO: Update the weights of each particle using a mult-variate Gaussian 
   *   distribution. You can read more about this distribution here: 
   *   https://en.wikipedia.org/wiki/Multivariate_normal_distribution
   * NOTE: The observations are given in the VEHICLE'S coordinate system. 
   *   Your particles are located according to the MAP'S coordinate system. 
   *   You will need to transform between the two systems. Keep in mind that
   *   this transformation requires both rotation AND translation (but no scaling).
   *   The following is a good resource for the theory:
   *   https://www.willamette.edu/~gorr/classes/GeneralGraphics/Transforms/transforms2d.htm
   *   and the following is a good resource for the actual equation to implement
   *   (look at equation 3.33) http://planning.cs.uiuc.edu/node99.html
   */

  for (auto& particle : particles)
  {
    // First extract the landmarks inside the sensor range
    vector<LandmarkObs> predicted_landmarks;

    for (auto& map_landmark : map_landmarks.landmark_list)
    {
      double d = dist(particle.x, particle.y, map_landmark.x_f, map_landmark.y_f);
      if (d <= sensor_range)
      {
        LandmarkObs landmark;
        landmark.id = map_landmark.id_i;
        landmark.x = map_landmark.x_f;
        landmark.y = map_landmark.y_f;
        predicted_landmarks.push_back(landmark);
      }
    }

    // Transform the observations to map coordinate
    vector<LandmarkObs> observed_landmarks;
    for (auto& obs : observations)
    {
      LandmarkObs landmark;
      landmark.id = obs.id;
      landmark.x = obs.x * cos(particle.theta) - obs.y * sin(particle.theta) + particle.x;
      landmark.y = obs.x * sin(particle.theta) + obs.y * cos(particle.theta) + particle.y;
      observed_landmarks.push_back(landmark);
    }

    // Use the nearest neighbor to associate the observation to prediction 
    dataAssociation(predicted_landmarks, observed_landmarks);

    // Calculate the particle weight using multivariant-gaussian distribution function
    double weight = 1;
    double std_x = std_landmark[0];
    double std_y = std_landmark[1];

    for (auto& obs : observed_landmarks)
    {
      int idx = obs.id;
      double mu_x = predicted_landmarks[idx].x;
      double mu_y = predicted_landmarks[idx].y;
      weight *= multivar_gauss(obs.x, mu_x, std_x, obs.y, mu_y, std_y);
    }

    // Update the weight of the particle
    particle.weight = weight;    
  }

  // Normalized the weights
  double sum_weights;
  for (const auto& particle : particles)
  {
    sum_weights += particle.weight;
  }

  std::cout << "sum of the weights = " << sum_weights << std::endl;

  for (auto& particle : particles)
  {
    particle.weight /= sum_weights;
  }
}

void ParticleFilter::resample() {
  /**
   * TODO: Resample particles with replacement with probability proportional 
   *   to their weight. 
   * NOTE: You may find std::discrete_distribution helpful here.
   *   http://en.cppreference.com/w/cpp/numeric/random/discrete_distribution
   */

}

void ParticleFilter::SetAssociations(Particle& particle, 
                                     const vector<int>& associations, 
                                     const vector<double>& sense_x, 
                                     const vector<double>& sense_y) {
  // particle: the particle to which assign each listed association, 
  //   and association's (x,y) world coordinates mapping
  // associations: The landmark id that goes along with each listed association
  // sense_x: the associations x mapping already converted to world coordinates
  // sense_y: the associations y mapping already converted to world coordinates
  particle.associations= associations;
  particle.sense_x = sense_x;
  particle.sense_y = sense_y;
}

string ParticleFilter::getAssociations(Particle best) {
  vector<int> v = best.associations;
  std::stringstream ss;
  copy(v.begin(), v.end(), std::ostream_iterator<int>(ss, " "));
  string s = ss.str();
  s = s.substr(0, s.length()-1);  // get rid of the trailing space
  return s;
}

string ParticleFilter::getSenseCoord(Particle best, string coord) {
  vector<double> v;

  if (coord == "X") {
    v = best.sense_x;
  } else {
    v = best.sense_y;
  }

  std::stringstream ss;
  copy(v.begin(), v.end(), std::ostream_iterator<float>(ss, " "));
  string s = ss.str();
  s = s.substr(0, s.length()-1);  // get rid of the trailing space
  return s;
}