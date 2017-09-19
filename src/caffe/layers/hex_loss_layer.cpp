#include <algorithm>
#include <cfloat>
#include <vector>

#include "caffe/layers/hex_loss_layer.hpp"
#include "caffe/util/math_functions.hpp"
#include "caffe/util/HEXGraph.hpp"

namespace caffe {

template <typename Dtype>
void HexLossLayer<Dtype>::LayerSetUp(
    const vector<Blob<Dtype>*>& bottom, const vector<Blob<Dtype>*>& top) {
    LossLayer<Dtype>::LayerSetUp(bottom, top);
 
    HexLossParameter param = this->layer_param_.hex_loss_param(); 
    G_file_ = this->layer_param_.hex_loss_param().g_file();
    label_name_file_ = param.label_name_file();
    node_nums_ = param.node_nums();
    //LOG(INFO) << G_file_ << " " << label    
    hex_ = HexGraph<Dtype>(G_file_, label_name_file_, node_nums_);
    hex_.Init();
    hex_.ProduceG();
    hex_.ListStatesSpace();

    LOG(INFO) << "HexGraph Produced!";
    CHECK_EQ(node_nums_, bottom[0]->channels());
}

template <typename Dtype>
void HexLossLayer<Dtype>::Reshape(
    const vector<Blob<Dtype>*>& bottom, const vector<Blob<Dtype>*>& top) {
    LossLayer<Dtype>::Reshape(bottom, top);	
    //top[0]->Reshape(1);
}


//template <typename Dtype>
//Dtype HexLossLayer<Dtype>::get_normalizer(
//    LossParameter_NormalizationMode normalization_mode, int valid_count) {
//}

template <typename Dtype>
void HexLossLayer<Dtype>::Forward_cpu(
    const vector<Blob<Dtype>*>& bottom, const vector<Blob<Dtype>*>& top) {
  // The forward pass computes the softmax prob values.
    //const Dtype* bottom_data = bottom[0]->cpu_data();	//score
    //const Dtype* label = bottom[1]->cpu_data();
    //LOG(INFO) << "#";
    //Dtype* loss = top[0]->mutable_cpu_data();
    //LOG(INFO) << "?";
    hex_.Forward_cpu(bottom, top);	
}

template <typename Dtype>
void HexLossLayer<Dtype>::Backward_cpu(const vector<Blob<Dtype>*>& top,
    const vector<bool>& propagate_down, const vector<Blob<Dtype>*>& bottom) {
    if (propagate_down[1]) {
	LOG(FATAL) << this->type() << " Layer cannot backpropagate to label inputs.";
    }
    if (propagate_down[0]) {
	hex_.Backward_cpu(bottom, top);
    }
}

#ifdef CPU_ONLY
STUB_GPU(HexLossLayer);
#endif

INSTANTIATE_CLASS(HexLossLayer);
REGISTER_LAYER_CLASS(HexLoss);

}  // namespace caffe
