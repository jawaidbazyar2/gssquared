void generate_filters(int num_taps);
void init_display_configuration();
// Point-sample Y/I/Q taps 0..num_taps for a sample rate of rate_scale x 4fsc:
// chroma-trap luma (Gaussian, 1/e at luma_bandwidth Hz, null at the subcarrier)
// and windowed-sinc I / Q with nulls at the subcarrier and twice it.
void generate_trap_point_filters(float rate_scale, int num_taps, float luma_bandwidth,
                                 float i_bandwidth, float q_bandwidth, float out[][3]);