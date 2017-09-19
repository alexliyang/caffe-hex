#include <functional>
#include <utility>
#include <vector>

#include "caffe/layers/hex_accuracy_layer.hpp"
#include "caffe/util/math_functions.hpp"

namespace caffe {

template <typename Dtype>
void HexAccuracyLayer<Dtype>::LayerSetUp(
  const vector<Blob<Dtype>*>& bottom, const vector<Blob<Dtype>*>& top) {
  top_k_ = this->layer_param_.hex_accuracy_param().top_k();

  has_ignore_label_ =
    this->layer_param_.hex_accuracy_param().has_ignore_label();
  if (has_ignore_label_) {
    ignore_label_ = this->layer_param_.hex_accuracy_param().ignore_label();
  }
  
  HexAccuracyParameter param = this->layer_param_.hex_accuracy_param();
  G_file_ = param.g_file();
  label_name_file_ = param.label_name_file();
  node_nums_ = param.node_nums();
  //LOG(INFO) << G_file_ << " " << label    
  hex_ = HexGraph<Dtype>(G_file_, label_name_file_, node_nums_);
  hex_.Init();
  hex_.ProduceG();
}

template <typename Dtype>
void HexAccuracyLayer<Dtype>::Reshape(
  const vector<Blob<Dtype>*>& bottom, const vector<Blob<Dtype>*>& top) {
  CHECK_LE(top_k_, bottom[0]->count() / bottom[1]->count())
      << "top_k must be less than or equal to the number of classes.";
  label_axis_ =
      bottom[0]->CanonicalAxisIndex(this->layer_param_.accuracy_param().axis());
  
  outer_num_ = bottom[0]->count(0, label_axis_); //n
  inner_num_ = bottom[0]->count(label_axis_ + 1); //w*h
  CHECK_EQ(outer_num_ * inner_num_, bottom[1]->count()) 
      << "Number of labels must match number of predictions; "
      << "e.g., if label axis == 1 and prediction shape is (N, C, H, W), "
      << "label count (number of labels) must be N*H*W, "
      << "with integer values in {0, 1, ..., C-1}.";
  vector<int> top_shape(0);  // Accuracy is a scalar; 0 axes.
  top[0]->Reshape(top_shape);
  if (top.size() > 1) {
    // Per-class accuracy is a vector; 1 axes.
    vector<int> top_shape_per_class(1);
    top_shape_per_class[0] = bottom[0]->shape(label_axis_);
    top[1]->Reshape(top_shape_per_class);
    nums_buffer_.Reshape(top_shape_per_class);
  }
}

template <typename Dtype>
void HexAccuracyLayer<Dtype>::Forward_cpu(const vector<Blob<Dtype>*>& bottom,
    const vector<Blob<Dtype>*>& top) {
  hex_.CalLabels(bottom, node_);
  
  Dtype accuracy = 0;
  //const Dtype* bottom_data = bottom[0]->cpu_data();
  const Dtype* bottom_label = bottom[1]->cpu_data();
  const int dim = node_.channels(); //c
  const int num_labels = node_.shape(label_axis_); //num_labels
  vector<Dtype> maxval(top_k_+1); 
  vector<int> max_id(top_k_+1);
  
  if (top.size() > 1) {
    caffe_set(nums_buffer_.count(), Dtype(0), nums_buffer_.mutable_cpu_data());
    caffe_set(top[1]->count(), Dtype(0), top[1]->mutable_cpu_data());
  }
  int count = 0;
  
  //hex_.CalLabels(bottom, node_);
  const Dtype* node_data = node_.cpu_data();
  
  for (int i = 0; i < outer_num_; ++i) { //n
    for (int j = 0; j < inner_num_; ++j) { //w * h = 1
      const int label_value =
          static_cast<int>(bottom_label[i * inner_num_ + j]); //label_value
      if (has_ignore_label_ && label_value == ignore_label_) {
        continue;
      }
      if (top.size() > 1) ++nums_buffer_.mutable_cpu_data()[label_value]; //top>1 not consider
      DCHECK_GE(label_value, 0); 
      DCHECK_LT(label_value, num_labels); 
      // Top-k accuracy
      std::vector<std::pair<Dtype, int> > bottom_data_vector;

      for (int k = 0; k < num_labels; ++k) {
        bottom_data_vector.push_back(std::make_pair(
            node_data[i * dim + k * inner_num_ + j], k));
      }
      std::partial_sort(
          bottom_data_vector.begin(), bottom_data_vector.begin() + top_k_,
          bottom_data_vector.end(), std::greater<std::pair<Dtype, int> >());
      // check if true label is in top k predictions
      for (int k = 0; k < top_k_; k++) {
        if (bottom_data_vector[k].second == label_value) {
          ++accuracy;
          if (top.size() > 1) ++top[1]->mutable_cpu_data()[label_value];
          break;
        }
      }
      ++count;
    }
  }

  //LOG(INFO) << "Accuracy: " << accuracy;
  top[0]->mutable_cpu_data()[0] = accuracy / count;
  if (top.size() > 1) {
    for (int i = 0; i < top[1]->count(); ++i) {
      top[1]->mutable_cpu_data()[i] =
          nums_buffer_.cpu_data()[i] == 0 ? 0
          : top[1]->cpu_data()[i] / nums_buffer_.cpu_data()[i];
    }
  }
  //LOG(INFO) << "???";
  // Accuracy layer should not be used as a loss function.
}

INSTANTIATE_CLASS(HexAccuracyLayer);
REGISTER_LAYER_CLASS(HexAccuracy);

}  // namespace caffe
