#pragma once

#include <mutex>
#include <tuple>
#include <utility>
#include <vector>

#include "chunked_vector.hpp"
#include "timeseries.hpp"

class TimeSeriesDenseSampler
{
  public:
    std::vector<std::pair<int, int>> sample(unsigned int rows, unsigned int start, unsigned int end);
};

struct DataStore
{
  double sum;
  double min;
  double max;
};

/**
 * @brief A densely packed time series with a fixed data rate.
 * This time series is useful if you
 */
class TimeSeriesDense : public TimeSeries
{
  public:
    TimeSeriesDense(double start, double interval);
    virtual ~TimeSeriesDense() = default;

    std::size_t get_samples(TSSample *samples,
                            double timestamp_start,
                            double bin_width,
                            std::size_t num_samples) const override;
    TSSample get_sample(double timestamp, double bin_width) override;
    std::pair<double, double> get_span() const override;
    void push_sample(double value);
    void push_samples(const std::vector<double> &value);

  private:
    std::tuple<double, double, double> _reduce(std::size_t, std::size_t) const;
    mutable std::recursive_mutex _mut;
    std::vector<ChunkedVector<DataStore, 1024>> _data;
    double _interval;
    double _start;
};
