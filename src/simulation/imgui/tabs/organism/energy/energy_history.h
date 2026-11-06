#pragma once

// Fixed-capacity ring buffer of organism energy samples for the Energy tab.
// Values are stored as fractions (0..1) normalised at sample time, so history
// stays comparable when cells are added or removed.
struct EnergyHistory
{
	static constexpr int capacity = 4096;

	enum Series : int { Energy, Nutrients, Integrity, CellMin, CellMean, CellMax, SeriesCount };

	struct Sample
	{
		float tick;               // monotonic UI-side tick (exact in float up to ~16.7M)
		float v[SeriesCount];
	};

	void clear() { head_ = 0; count_ = 0; }
	int  size()  const { return count_; }
	bool empty() const { return count_ == 0; }

	// k = 0 is the oldest sample, k = size()-1 the newest.
	const Sample& at(int k) const { return buf_[(head_ + k) % capacity]; }
	const Sample& newest() const { return at(count_ - 1); }

	void push(const Sample& s)
	{
		if (count_ < capacity)
			buf_[(head_ + count_++) % capacity] = s;
		else
		{
			buf_[head_] = s;
			head_ = (head_ + 1) % capacity;
		}
	}

	// Least-squares slope (value change per tick) over the last `max_samples` samples.
	// Returns 0 until there are enough samples to say anything.
	float slope(Series s, int max_samples) const
	{
		const int n = count_ < max_samples ? count_ : max_samples;
		if (n < 4) return 0.f;

		const int first = count_ - n;
		const double x0 = at(first).tick;
		double sx = 0, sy = 0, sxx = 0, sxy = 0;
		for (int k = first; k < count_; ++k)
		{
			const double x = at(k).tick - x0;
			const double y = at(k).v[s];
			sx += x; sy += y; sxx += x * x; sxy += x * y;
		}
		const double denom = n * sxx - sx * sx;
		if (denom < 1e-9) return 0.f;
		return static_cast<float>((n * sxy - sx * sy) / denom);
	}

private:
	Sample buf_[capacity]{};
	int    head_ = 0;   // index of the oldest sample
	int    count_ = 0;
};