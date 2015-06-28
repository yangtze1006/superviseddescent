/*
 * superviseddescent: A C++11 implementation of the supervised descent
 *                    optimisation method
 * File: rcr/helpers.hpp
 *
 * Copyright 2015 Patrik Huber
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once

#ifndef HELPERS_HPP_
#define HELPERS_HPP_

#include "landmark.hpp"

#include "opencv2/core/core.hpp"

#include <vector>
#include <chrono>

namespace rcr {

/**
 * Convert the given landmarks to a cv::Mat of one row in
 * the form [x_0, ..., x_n, y_0, ..., y_n]. The matrix row
 * will have type CV_32FC1.
 *
 * Note: we could write this more generic if needed (template<class LandmarkType>)
 * but for now it's unnecessary complicated.
 *
 * @param[in] landmarks The landmarks to convert to a cv::Mat row.
 * @return A cv::Mat with one row consisting of the given landmarks.
 */
cv::Mat to_row(LandmarkCollection<cv::Vec2f> landmarks)
{
	// landmarks.size() must be <= max_int
	auto num_landmarks = static_cast<int>(landmarks.size());
	cv::Mat row(1, num_landmarks * 2, CV_32FC1);
	for (int i = 0; i < num_landmarks; ++i) {
		row.at<float>(i) = landmarks[i].coordinates[0];
		row.at<float>(i + num_landmarks) = landmarks[i].coordinates[1];
	}
	return row;
}

/**
 * Convert the given model instance (a row of 2D landmarks in the
 * form [x_0, ..., x_n, y_0, ..., y_n]) to a LandmarkCollection
 * and use the identifiers in \p model_landmarks_list as landmark
* names (in order).
 *
 * @param[in] model_instance A row of 2D landmarks (e.g. an instance of a model).
 * @return A LandmarkCollection with the landmarks and their names.
 */
LandmarkCollection<cv::Vec2f> to_landmark_collection(cv::Mat model_instance, std::vector<std::string> model_landmarks_list)
{
	LandmarkCollection<cv::Vec2f> collection;
	auto num_landmarks = model_instance.cols / 2;
	assert(num_landmarks == model_landmarks_list.size()); // shouldn't be an assert, not a programming/internal error.
	for (int i = 0; i < num_landmarks; ++i) {
		collection.emplace_back(Landmark<cv::Vec2f>{ model_landmarks_list[i], cv::Vec2f{ model_instance.at<float>(i), model_instance.at<float>(i + num_landmarks) } });
	}
	return collection;
}

/**
 * Draws the given landmarks into the image.
 *
 * @param[in] image An image to draw into.
 * @param[in] landmarks The landmarks to draw, in the format [x_0, ... , x_n, y_0, ... , y_n].
 * @param[in] color Color of the landmarks to be drawn.
 */
void draw_landmarks(cv::Mat image, cv::Mat landmarks, cv::Scalar color = cv::Scalar(0.0, 255.0, 0.0))
{
	auto num_landmarks = std::max(landmarks.cols, landmarks.rows) / 2;
	for (int i = 0; i < num_landmarks; ++i) {
		cv::circle(image, cv::Point2f(landmarks.at<float>(i), landmarks.at<float>(i + num_landmarks)), 2, color);
	}
}

/**
 * Draws the given landmarks into the image.
 *
 * @param[in] image An image to draw into.
 * @param[in] landmarks The landmarks to draw.
 * @param[in] color Color of the landmarks to be drawn.
 */
void draw_landmarks(cv::Mat image, LandmarkCollection<cv::Vec2f> landmarks, cv::Scalar color = cv::Scalar(0.0, 255.0, 0.0))
{
	draw_landmarks(image, to_row(landmarks), color);
}

// checks overlap...
// Possible better names: check_equal, check_is_true_positive, overlap...
bool check_face(std::vector<cv::Rect> detected_faces, LandmarkCollection<cv::Vec2f> groundtruth_landmarks)
{
	// If no face is detected, return immediately:
	if (detected_faces.empty()) {
		return false;
	}

	bool is_true_positive = true;
	// TODO: Need to make the following better!
	// for now, if the ground-truth landmarks 37 (reye_oc), 46 (leye_oc) and 58 (mouth_ll_c) are inside the face-box
	// (should add: _and_ the face-box is not bigger than IED*2 or something)
	//assert(groundtruthLandmarks.size() == 68); // only works with ibug-68 so far...
	// for now: user should make sure we have 37, 46, 58.
	
	for (const auto& lm : groundtruth_landmarks) {
		if (lm.name == "37" || lm.name == "46" || lm.name == "58") {
			if (!detected_faces[0].contains(cv::Point(lm.coordinates))) {
				is_true_positive = false;
				break; // if any LM is not inside, skip this training image
				// Note: improvement: if the first face-box doesn't work, try the other ones
			}
		}
	}

	return is_true_positive;
}

// Calculate the IED from one or several identifiers.
// Several is necessary because sometimes (e.g. ibug) doesn't define the eye center.
// throws if any of given ids not present in lms. => Todo: Think about if we should throw or use optional<>.
double get_ied(LandmarkCollection<cv::Vec2f> lms, std::vector<std::string> right_eye_identifiers, std::vector<std::string> left_eye_identifiers)
{
	// Calculate the inter-eye distance. Which landmarks to take for that is specified in the config, it
	// might be one or two, and we calculate the average of them (per eye). For example, it might be the outer eye-corners.
	cv::Vec2f right_eye_center(0.0f, 0.0f);
	for (const auto& right_eye_identifyer : right_eye_identifiers) {
		LandmarkCollection<cv::Vec2f>::const_iterator element;
		if ((element = std::find_if(begin(lms), end(lms), [&right_eye_identifyer](const Landmark<cv::Vec2f>& landmark) { return landmark.name == right_eye_identifyer; })) == end(lms)) {
			throw std::runtime_error("one of given rightEyeIdentifiers ids not present in lms");
		}
		right_eye_center += element->coordinates;
	}
	right_eye_center /= static_cast<float>(right_eye_identifiers.size());
	cv::Vec2f left_eye_center(0.0f, 0.0f);
	for (const auto& leftEyeIdentifyer : left_eye_identifiers) {
		LandmarkCollection<cv::Vec2f>::const_iterator element;
		if ((element = std::find_if(begin(lms), end(lms), [&leftEyeIdentifyer](const Landmark<cv::Vec2f>& landmark) { return landmark.name == leftEyeIdentifyer; })) == end(lms)) {
			throw std::runtime_error("one of given leftEyeIdentifiers ids not present in lms");
		}
		left_eye_center += element->coordinates;
	}
	left_eye_center /= static_cast<float>(left_eye_identifiers.size());
	cv::Scalar inter_eye_distance = cv::norm(right_eye_center, left_eye_center, cv::NORM_L2);
	return inter_eye_distance[0];
};

class PartialPivLUSolveSolverDebug
{
public:
	// Note: we should leave the choice of inverting A or AtA to the solver.
	// But this also means we need to pass through the regularisation params.
	// We can't just pass a cv::Mat regularisation because the dimensions for
	// regularising A and AtA are different.
	cv::Mat solve(cv::Mat data, cv::Mat labels, superviseddescent::Regulariser regulariser)
	{
		using cv::Mat;
		using std::cout;
		using std::endl;
		using RowMajorMatrixXf = Eigen::Matrix<float, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;
		using namespace std::chrono;
		time_point<system_clock> start, end;

		// Map the cv::Mat data and labels to Eigen matrices:
		Eigen::Map<RowMajorMatrixXf> A_Eigen(data.ptr<float>(), data.rows, data.cols);
		Eigen::Map<RowMajorMatrixXf> labels_Eigen(labels.ptr<float>(), labels.rows, labels.cols);

		start = system_clock::now();
		RowMajorMatrixXf AtA_Eigen = A_Eigen.transpose() * A_Eigen;
		end = system_clock::now();
		cout << "At * A (ms): " << duration_cast<milliseconds>(end - start).count() << endl;

		// Note: This is a bit of unnecessary back-and-forth mapping, just for the regularisation:
		Mat AtA_Map(static_cast<int>(AtA_Eigen.rows()), static_cast<int>(AtA_Eigen.cols()), CV_32FC1, AtA_Eigen.data());
		Mat regularisationMatrix = regulariser.get_matrix(AtA_Map, data.rows);
		Eigen::Map<RowMajorMatrixXf> reg_Eigen(regularisationMatrix.ptr<float>(), regularisationMatrix.rows, regularisationMatrix.cols);

		Eigen::DiagonalMatrix<float, Eigen::Dynamic> reg_Eigen_diag(regularisationMatrix.rows);
		Eigen::VectorXf diagVec(regularisationMatrix.rows);
		for (int i = 0; i < diagVec.size(); ++i) {
			diagVec(i) = regularisationMatrix.at<float>(i, i);
		}
		reg_Eigen_diag.diagonal() = diagVec;
		start = system_clock::now();
		AtA_Eigen = AtA_Eigen + reg_Eigen_diag.toDenseMatrix();
		end = system_clock::now();
		cout << "AtA + Reg (ms): " << duration_cast<milliseconds>(end - start).count() << endl;

		// Perform a PartialPivLU:
		start = system_clock::now();
		Eigen::PartialPivLU<RowMajorMatrixXf> qrOfAtA(AtA_Eigen);
		end = system_clock::now();
		cout << "Decomposition (ms): " << duration_cast<milliseconds>(end - start).count() << endl;
		start = system_clock::now();
		//RowMajorMatrixXf AtAInv_Eigen = qrOfAtA.inverse();
		RowMajorMatrixXf x_Eigen = qrOfAtA.solve(A_Eigen.transpose() * labels_Eigen);
		//RowMajorMatrixXf x_Eigen = AtA_Eigen.partialPivLu.solve(A_Eigen.transpose() * labels_Eigen);
		end = system_clock::now();
		cout << "solve() (ms): " << duration_cast<milliseconds>(end - start).count() << endl;

		// x = (AtAReg)^-1 * At * b:
		start = system_clock::now();
		//RowMajorMatrixXf x_Eigen = AtAInv_Eigen * A_Eigen.transpose() * labels_Eigen;
		end = system_clock::now();
		cout << "AtAInv * At * b (ms): " << duration_cast<milliseconds>(end - start).count() << endl;

		// Map the resulting x back to a cv::Mat by creating a Mat header:
		Mat x(static_cast<int>(x_Eigen.rows()), static_cast<int>(x_Eigen.cols()), CV_32FC1, x_Eigen.data());

		// We have to copy the data because the underlying data is managed by Eigen::Matrix x_Eigen, which will go out of scope after we leave this function:
		return x.clone();
		//return qrOfAtA.isInvertible();
	};
};

} /* namespace rcr */

#endif /* HELPERS_HPP_ */