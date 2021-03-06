#ifndef KDTREE_KD_TREE_H
#define KDTREE_KD_TREE_H

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <memory>
#include <queue>
#include <string>
#include <vector>

const float INITIAL_LIMIT = INT64_MAX;

template <class Object> class KDTree;

template <class Object> class KDTreeBuilder;

template <int Dimension> class KDTreePlaceable {
public:
  virtual std::array<float, Dimension> GetPoint() const = 0;

  constexpr static int GetDimension() { return Dimension; }
};

enum class SplitType { Median };

template <class Object> class KDTree {
public:
  KDTree() = delete;

  explicit KDTree(std::vector<Object> &&objects)
      : objects_(std::move(objects)),
        root_(std::make_unique<TreeNode>(objects_)),
        dim_(Object::GetDimension()),
        gap_{} {
      gap_.fill(0);
  }

  KDTree(KDTree &&tree)
      : objects_(std::move(tree.objects_)), root_(tree.root_), dim_(tree.dim_), gap_(tree.gap_) {
  }

  bool SaveToFile(const std::string &filename) const;

  std::vector<const Object *> SearchClosest(const Object &obj, float eps,
                                                  int amount, std::function<bool(const Object &, const Object &)> comparator) const {
    std::array<float, Object::GetDimension()> eps_array;
    eps_array.fill(eps);
    return SearchClosest(obj, eps_array, amount, comparator);
  }

  std::vector<const Object *> SearchClosest(
      const Object &obj, const std::array<float, Object::GetDimension()> &eps,
      int amount,
      std::function<bool(const Object &, const Object &)> comparator) const {
    std::queue<TreeNode *> queue;
    queue.push(root_.get());

    std::vector<const Object *> result;
    std::map<size_t, bool> registered_nodes;

    while (!queue.empty()) {
      TreeNode *current_node = queue.front();

      if (current_node->IsLeaf()) {
        for (size_t ind : current_node->objects_) {
          if (registered_nodes.find(ind) == registered_nodes.end()) {
            result.push_back(&(objects_[ind]));
            registered_nodes[ind] = true;
          }
        }
      } else {
        TreeNode *left = current_node->GetLeft();
        TreeNode *right = current_node->GetRight();

        if (left->IsCovering(obj, eps)) {
          queue.push(left);
        }

        if (right->IsCovering(obj, eps)) {
          queue.push(right);
        }
      }

      queue.pop();
    }

    auto transit = [&comparator](const Object *first, const Object *second) {
      return comparator(*first, *second);
    };
    std::sort(result.begin(), result.end(), transit);

    if (result.size() <= amount) {
      return result;
    }

    result.resize(amount);

    return result;
  }

  size_t GetSize() const { return objects_.size(); }

  std::array<float, Object::GetDimension()> GetGap() const {
      return gap_;
  }

  const Object& operator[](size_t index) const {
      return objects_[index];
  }

  auto begin() const {
      return objects_.begin();
  }

  auto end() const {
      return objects_.end();
  }

  size_t size() const {
      return objects_.size();
  }

private:
  struct TreeNode {
    TreeNode(const std::vector<Object> &objects)
        : split_dimension_(0), depth_(0) {
      int dim = Object::GetDimension();

      std::array<float, Object::GetDimension()> root_point_max;
      std::array<float, Object::GetDimension()> root_point_min;
      root_point_max.fill(-INITIAL_LIMIT);
      root_point_min.fill(INITIAL_LIMIT);

      for (const Object &obj : objects) {
        std::array<float, Object::GetDimension()> point = obj.GetPoint();

        for (size_t d = 0; d < dim; ++d) {
          if (root_point_max[d] < point[d]) {
            root_point_max[d] = point[d];
          }
          if (root_point_min[d] > point[d]) {
            root_point_min[d] = point[d];
          }
        }
      }

      point_max_ = root_point_max;
      point_min_ = root_point_min;

      objects_.reserve(objects.size());
      for (size_t obj = 0; obj < objects.size(); ++obj) {
        objects_.push_back(obj);
      }
    }

    TreeNode(size_t split_dimension,
             const std::array<float, Object::GetDimension()> &point_max,
             const std::array<float, Object::GetDimension()> &point_min,
             std::vector<size_t> &&objects, int depth = -1)
        : split_dimension_(split_dimension), left_(nullptr), right_(nullptr),
          point_max_(point_max), point_min_(point_min),
          objects_(std::move(objects)), depth_(depth) {}

    bool IsLeaf() const { return ((left_ == nullptr) && (right_ == nullptr)); }

    size_t GetObjectsAmount() const { return (objects_.size()); }

    float GetSpace() const {
      float space = 1;
      for (int dim = 0; dim < Object::GetDimension(); ++dim) {
        space *= std::abs(point_max_[dim] - point_min_[dim]);
      }

      return space;
    }

    TreeNode *GetLeft() const { return left_.get(); }

    TreeNode *GetRight() const { return right_.get(); }

    bool
    IsCovering(const Object &obj,
               const std::array<float, Object::GetDimension()> &eps) const {
      const std::array<float, Object::GetDimension()> point = obj.GetPoint();
      for (size_t dim = 0; dim < Object::GetDimension(); ++dim) {
        if (point[dim] < (point_min_[dim] - eps[dim]) ||
            point[dim] > (point_max_[dim] + eps[dim])) {
          return false;
        }
      }
      return true;
    }

    size_t split_dimension_;
    std::unique_ptr<TreeNode> left_;

    std::unique_ptr<TreeNode> right_;
    std::array<float, Object::GetDimension()> point_max_;

    std::array<float, Object::GetDimension()> point_min_;
    std::vector<size_t> objects_;

    int depth_;
  };

private:
  std::array<float, Object::GetDimension()>
  GetMaxLimit(const std::vector<size_t> &arr) const {
    std::array<float, Object::GetDimension()> res;
    res.fill(-INITIAL_LIMIT);

    for (size_t ind : arr) {
      for (int dim = 0; dim < dim_; ++dim) {
        auto val = objects_[ind].GetPoint()[dim];
        if (res[dim] < val) {
          res[dim] = val;
        }
      }
    }

    return res;
  }

  std::array<float, Object::GetDimension()>
  GetMinLimit(const std::vector<size_t> &arr) const {
    std::array<float, Object::GetDimension()> res;
    res.fill(INITIAL_LIMIT);

    for (size_t ind : arr) {
      for (int dim = 0; dim < dim_; ++dim) {
        auto val = objects_[ind].GetPoint()[dim];
        if (res[dim] > val) {
          res[dim] = val;
        }
      }
    }

    return res;
  }

  std::pair<TreeNode *, TreeNode *> SliceLeafMedian(TreeNode *leaf,
                                                    int max_depth) {
    if ((max_depth <= leaf->depth_) || (leaf->objects_.size() <= 1)) {
      return {nullptr, nullptr};
    }

    auto comparator = [this, dim = leaf->split_dimension_](size_t left,
                                                           size_t right) {
      return objects_[left].GetPoint()[dim] < objects_[right].GetPoint()[dim];
    };
    std::sort(leaf->objects_.begin(), leaf->objects_.end(), comparator);

    auto median = leaf->objects_.begin() + (leaf->objects_.size() / 2);
    std::vector<size_t> left_objects(leaf->objects_.begin(), median);
    std::vector<size_t> right_objects(median, leaf->objects_.end());

    leaf->left_ = std::make_unique<TreeNode>(
        (leaf->split_dimension_ + 1) % dim_, GetMaxLimit(left_objects),
        GetMinLimit(left_objects), std::move(left_objects), leaf->depth_ + 1);
    leaf->right_ = std::make_unique<TreeNode>(
        (leaf->split_dimension_ + 1) % dim_, GetMaxLimit(right_objects),
        GetMinLimit(right_objects), std::move(right_objects), leaf->depth_ + 1);
    leaf->objects_.clear();

    float dim_gap = leaf->right_->point_min_[leaf->split_dimension_] - leaf->left_->point_max_[leaf->split_dimension_];
    if (dim_gap > gap_[leaf->split_dimension_]) {
        gap_[leaf->split_dimension_] = dim_gap;
    }

    return {leaf->left_.get(), leaf->right_.get()};
  }

  const TreeNode *GetRoot() const { return root_.get(); }

private:
  friend class KDTreeBuilder<Object>;

private:
  const std::vector<Object> objects_;
  std::unique_ptr<TreeNode> root_;
  size_t dim_;
  std::array<float, Object::GetDimension()> gap_;
};

template <class Object> class KDTreeBuilder {
public:
  KDTreeBuilder(SplitType split_type, int max_depth = 1)
      : tree_(nullptr), split_type_(split_type),
        max_depth_(max_depth) {}

  void LoadObjects(std::vector<Object> &&objects) {
    tree_ = std::make_unique<KDTree<Object>>(std::move(objects));
  }

  std::unique_ptr<KDTree<Object>> &&BuildTree() {
    std::queue<class KDTree<Object>::TreeNode *> queue;
    queue.push(tree_->root_.get());

    while (!queue.empty()) {
      class KDTree<Object>::TreeNode *node = queue.front();
      std::pair<class KDTree<Object>::TreeNode *,
                class KDTree<Object>::TreeNode *>
          new_nodes = tree_->SliceLeafMedian(node, max_depth_);

      // a bit excessive but just in case
      if ((new_nodes.first != nullptr) && (new_nodes.second != nullptr)) {
        queue.push(new_nodes.first);
        queue.push(new_nodes.second);
      }

      queue.pop();
    }

    return std::move(tree_);
  }

  KDTree<Object> &&LoadFromFile(const std::string &filename);

private:
  std::unique_ptr<KDTree<Object>> tree_;
  SplitType split_type_;
  int max_depth_;
};

#endif // KDTREE_KD_TREE_H
