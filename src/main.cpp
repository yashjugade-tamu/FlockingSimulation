#include "raylib.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <random>
#include <string>
#include <vector>

// Debugging helper, enable when needed:
// #include <iostream>


using namespace std;
namespace {

constexpr int screen_width = 1440;
constexpr int screen_height = 900;
constexpr float pi = 3.14159265359f;
constexpr float cell_size = 2.0f;
constexpr float flight_min_x = -11.2f;

    constexpr float flight_max_x = 11.2f;
constexpr float flight_min_height = 0.75f;
constexpr float flight_max_height = 5.2f;
constexpr float flight_min_depth = 0.2f;
constexpr float flight_max_depth = 5.5f;

struct agent {
    Vector3 position{};
    Vector3 velocity{};

        float size = 1.0f;
            float wing_phase = 0.0f;
    Color tint{};
};

struct lamp 
{
    Vector2 position{};
    
    bool on = true;
    float phase = 0.0f;};

struct obstacle {
    Rectangle bounds{};
    Vector2 base{};
    float phase = 0.0f;
    
    float amplitude = 0.0f;
    float speed = 1.0f;
    bool vertical = false;};

enum class run_mode 
{ 
    cycle_lights, manual_lights, no_light_attraction };

// Spatial hash: builds local candidate lists so the student's neighbor-rule
// implementation can inspect nearby agents without scanning the whole flock.
class spatial_grid 
{
public:
    void rebuild(const vector<agent>& agents) {
        columns_ = 12;
        rows_ = 3;
        layers_ = 3;

        cells_.assign(static_cast<size_t>(columns_ * rows_ * layers_), {});
        for (int i = 0; i < static_cast<int>(agents.size()); ++i) {
        const int x = coordinate(agents[i].position.x, -12.0f, columns_);
            
        const int y = coordinate(agents[i].position.y, 0.0f, rows_);
            const int z = coordinate(agents[i].position.z, 0.0f, layers_);
            cells_[index(x, y, z)].push_back(i);
    }
}

    void query(Vector3 position, float radius, vector<int>& result) const 
    {
        result.clear();

        if (columns_ == 0 || rows_ == 0 || layers_ == 0) {
            return;
        }

        const int min_x = coordinate(position.x - radius, -12.0f, columns_);
        const int max_x = coordinate(position.x + radius, -12.0f, columns_);

        const int min_y = coordinate(position.y - radius, 0.0f, rows_);
        const int max_y = coordinate(position.y + radius, 0.0f, rows_);

        const int min_z = coordinate(position.z - radius, 0.0f, layers_);
        const int max_z = coordinate(position.z + radius, 0.0f, layers_);

        for (int z = min_z; z <= max_z; ++z) {
            for (int y = min_y; y <= max_y; ++y) {
                for (int x = min_x; x <= max_x; ++x) {
                    const auto& cell = cells_[index(x, y, z)];
                    result.insert(result.end(), cell.begin(), cell.end());
                }
        }
        }
    }

private:
    static int coordinate(float value, float origin, int count) 
    
    {
        return clamp(static_cast<int>(floor((value - origin) / cell_size)), 0, count - 1);
    }
    size_t index(int x, int y, int z) const {
        return static_cast<size_t>((z * rows_ + y) * columns_ + x);
    }
    int columns_ = 0;
    int rows_ = 0;
    int layers_ = 0;
    vector<vector<int>> cells_;
};

vector<agent> agents;

vector<lamp> lamps;

vector<obstacle> obstacles;
spatial_grid spatial_grid_instance;

  vector<int> nearby_agents;
mt19937 random_engine{20261012u};
 uniform_real_distribution<float> unit_random{0.0f,1.0f};

bool automatic_lights = true;
bool paused = false;

    bool obstacles_enabled = true;
bool simulation_started = false;
bool light_attraction_enabled = true ;
run_mode active_mode= run_mode::cycle_lights;
run_mode draft_mode =run_mode::cycle_lights;
bool editing_draft_population = false;
float simulation_time= 0.0f;
float light_period =30.0f;
 float draft_light_period=30.0f;

float neighbor_radius = 2.35f;
float maximum_speed = 2.25f;
float wind_strength = 12.0f;
float separation_weight =1.8f;
float alignment_weight= 0.8f;

float cohesion_weight =0.65f;

float light_weight = 1.15f;
float avoidance_weight = 3.4f;
int desired_agent_count =240;
int draft_agent_count =240;
string draft_population_text;
Camera3D camera{};
float camera_yaw =0.55f;
float camera_pitch = 0.48f;
float camera_distance = 25.0f;
Vector3 camera_target{0.0f, 1.2f, -0.5f};




Vector3 simulation_to_world(Vector2 p, float height) 
{
    return {(p.x - GetScreenWidth() * 0.5f) / 55.0f, height, (p.y - 580.0f) / 55.0f};
}



float random_range(float low, float high) 
{
    return low + (high -   low) * unit_random(random_engine);
}


float active_flight_ceiling() {
    if (light_attraction_enabled) 
    {
        return flight_max_height;
    }

    return 3.0f;
}



void reset_scene() {
    agents.clear();
    agents.reserve(500);
    for (int i = 0; i < desired_agent_count; ++i) {
        agent a;

        const float initial_height_max = light_attraction_enabled ? 3.2f : 2.6f;
        a.position = simulation_to_world({random_range(120.0f, static_cast<float>(GetScreenWidth() - 120)),
                                          random_range(591.0f, 882.0f)},
                                         random_range(0.8f, initial_height_max));
        const float angle = random_range(0.0f, 2.0f * pi);

        a.velocity = {cos(angle) * 0.62f, random_range(-0.12f, 0.12f), sin(angle) * 0.62f};
        a.size = random_range(0.75f, 1.3f);
        a.wing_phase = random_range(0.0f, 2.0f * pi);

        const unsigned char shade = static_cast<unsigned char>(random_range(0.0f, 55.0f));
        a.tint = {static_cast<unsigned char>(235 - shade), static_cast<unsigned char>(211 - shade),
                  static_cast<unsigned char>(153 - shade), 255};
        agents.push_back(a);
}
    simulation_time = 0.0f;
}



void initialize_scene() {
    lamps = {
        {{260.0f, 615.0f}, true, 0.0f},
        {{570.0f, 615.0f}, true, 0.8f},
        {{900.0f, 615.0f}, true, 1.6f},
        {{1220.0f, 615.0f}, true, 2.4f}
    };

    obstacles = {
        {{  490.0f, 717.0f, 132.0f, 66.0f}, {556.0f, 750.0f}, 0.0f, 260.0f, 0.30f, false},
        {{784.0f,   802.0f, 132.0f, 66.0f}, {850.0f, 835.0f}, pi, 260.0f, 0.30f, false}
    };
    reset_scene();
}

void update_lamps() {
    if (!automatic_lights) 
    {
          return;
    }

    if (lamps.empty()) 
    {
           return;
    }

    const float cycle_position = fmod(simulation_time, light_period) / light_period;
    const int active_lamp_index = static_cast<int>( cycle_position * static_cast<float>(lamps.size()));
    for (int i = 0; i < static_cast<int>(lamps.size()); ++i) 
    {
        lamps[i].on = i == active_lamp_index;
    }
}

void update_obstacles() {
    for (obstacle& obstacle : obstacles) {
        const float displacement = obstacle.amplitude * sin(simulation_time * obstacle.speed + obstacle.phase);
        if (obstacle.vertical) {
            obstacle.bounds.x = obstacle.base.x;
            obstacle.bounds.y = obstacle.base.y + displacement - obstacle.bounds.height * 0.5f;
        } 
        else 
        {

            obstacle.bounds.x = obstacle.base.x + displacement - obstacle.bounds.width * 0.5f;
            obstacle.bounds.y = obstacle.base.y - obstacle.bounds.height * 0.5f;
        }
    }
}



Vector3 add3(Vector3 a, Vector3 b) 
{

    return {a.x+b.x,a.y + b.y, a.z + b.z};
}




Vector3 subtract3(Vector3 a, Vector3 b) {

    return {a.x - b.x, a.y - b.y, a.z - b.z};
}


Vector3 multiply3(Vector3 a, float s) {

    return {a.x * s, a.y * s, a.z * s};
}

float length_squared3(Vector3 a) {

    return a.x * a.x + a.y * a.y + a.z * a.z;
}



float length3(Vector3 a) 
{
    return sqrt(length_squared3(a));
}

Vector3 normalized3(Vector3 a) 
{
    const float magnitude = length3(a);

    if (magnitude > 1.0e-5f) {
        return multiply3(a, 1.0f / magnitude);
    }

    return {0.0f, 0.0f, 0.0f};
}


Vector3 steering_toward(Vector3 position, Vector3 velocity, Vector3 target) 
{

    const Vector3 offset = subtract3(target, position);
    if (length_squared3(offset) < 0.0004f) {
        return {0.0f, 0.0f, 0.0f};
    }

    const Vector3 desired = multiply3(normalized3(offset), maximum_speed);
    return multiply3(subtract3(desired, velocity), 1.25f);
}





Vector3 compute_student_acceleration(size_t agent_index,const vector<int>& nearby,const vector<agent>& flock,const vector<lamp>& light_sources,const vector<obstacle>& moving_obstacles) {
    const agent& self = flock[agent_index];

    Vector3 separation{0.0f, 0.0f, 0.0f};

    Vector3 average_velocity{0.0f, 0.0f, 0.0f};
    Vector3 average_position{0.0f, 0.0f, 0.0f};

    int neighbors_in_range = 0;
    constexpr float separation_radius = 0.52f;
    const float neighbor_radius_sq = neighbor_radius * neighbor_radius;

    const float separation_radius_sq = separation_radius * separation_radius;

    // A 3D spatial hash supplies local candidates; the exact spherical radius
    // test defines the agent's perception neighborhood.
    
    for (const int candidate : nearby) {
        if (candidate == static_cast<int>(agent_index)) 
        {
            continue;
        }

        const agent& other = flock[static_cast<size_t>(candidate)];
        const Vector3 delta = subtract3(self.position, other.position);
        const float distance_sq = length_squared3(delta);
        if (distance_sq <= 1.0e-5f || distance_sq > neighbor_radius_sq) 
        {
            continue;
        }


        ++neighbors_in_range;
        average_velocity = add3(average_velocity, other.velocity);
        average_position = add3(average_position,  other.position);
        if (distance_sq < separation_radius_sq) 
        {
            separation = add3(separation, multiply3(normalized3(delta), 1.0f / sqrt(distance_sq)));
        }
    }

    Vector3 acceleration = multiply3(normalized3(separation), separation_weight * 1.6f);
    if (neighbors_in_range > 0) {
            average_velocity =multiply3(average_velocity, 1.0f / neighbors_in_range);
        average_position = multiply3(average_position, 1.0f / neighbors_in_range);
        const Vector3 align_desired = multiply3(normalized3(average_velocity), maximum_speed);
            acceleration = add3(acceleration, multiply3(subtract3(align_desired, self.velocity), alignment_weight * 1.15f));
        acceleration = add3(acceleration, multiply3(steering_toward(self.position, self.velocity, average_position), cohesion_weight));
    }

    // The lamp is a 3D target at the bulb, so moths can rise toward it.
    Vector3 light_direction{0.0f, 0.0f, 0.0f};
    float light_total = 0.0f;
    constexpr float light_radius = 9.5f;
    if (light_attraction_enabled) {
        for (const lamp& lamp : light_sources) {
            if (!lamp.on) {
                continue;
            }

            const Vector3 target =simulation_to_world({lamp.position.x - 55.0f, lamp.position.y - 3.0f}, 4.48f);
            const Vector3 to_light = subtract3(target, self.position);

            const float distance =length3(to_light);
            if (distance < 1.0f || distance >=light_radius) 
                {
                continue;
            }

            const float influence=(1.0f- distance /light_radius) /(1.0f+ distance / 2.8f);
            light_direction = add3(light_direction,multiply3(normalized3(to_light), influence));
            light_total += influence;
        }
    }
    if (light_attraction_enabled && light_total > 1.0e-4f) 
    {
        
        const Vector3 preferred = normalized3(light_direction);
        
        const Vector3 desired = multiply3(preferred, maximum_speed * min(1.0f, light_total * 2.0f));
        acceleration = add3(acceleration, multiply3(subtract3(desired, self.velocity), light_weight * 1.4f));
    }

    // Wind drifts across the street in world X; the UI value is in screen-scale units.
    acceleration.x += (wind_strength / 55.0f - self.velocity.x) * 0.20f;

    // Compare the moth's and each moving bus's predicted paths over one second.
    constexpr float avoidance_horizon = 1.0f;
   
     constexpr int prediction_samples =6;
    constexpr float clearance =0.9f;
    for (const obstacle& obstacle : moving_obstacles) {
        const float half_x =obstacle.bounds.width * 1.08f / 110.0f + clearance;
        const float half_z = obstacle.bounds.height / 110.0f + clearance;
         constexpr float obstacle_height =2.31f;
        const float top_y =obstacle_height + clearance;
        Vector3 selected_escape{0.0f,0.0f,0.0f};
        Vector3 selected_bus_velocity{0.0f, 0.0f, 0.0f};
        bool predicted_contact = false;

        for (int sample = 0; sample < prediction_samples; ++sample) {
            const float future_time = avoidance_horizon * sample / (prediction_samples - 1);
            const float phase = (simulation_time + future_time) * obstacle.speed + obstacle.phase;
            
            const float displacement = obstacle.amplitude * sin(phase);
            
            const float center_screen_x = obstacle.vertical ? obstacle.base.x + obstacle.bounds.width * 0.5f
                                                            : obstacle.base.x + displacement;
            const float center_screen_y = obstacle.vertical ? obstacle.base.y + displacement
                                                            : obstacle.base.y;
            
                                                            const float center_x = (center_screen_x - GetScreenWidth() * 0.5f) / 55.0f;
            const float center_z = (center_screen_y - 580.0f) / 55.0f;
            const float left = center_x - half_x, right = center_x + half_x;
            const float near_z = center_z - half_z, far_z = center_z + half_z;
            
            const Vector3 predicted_moth = add3(self.position, multiply3(self.velocity, future_time));
            const Vector3 closest{clamp(predicted_moth.x, left, right),
                                  clamp(predicted_moth.y, 0.0f, top_y),
                                  clamp(predicted_moth.z, near_z, far_z)};
                    const Vector3 away = subtract3(predicted_moth, closest);
            const float distance = length3(away);
                    const bool inside = predicted_moth.x >= left && predicted_moth.x <= right &&
                                 predicted_moth.y >= 0.0f && predicted_moth.y <= top_y &&
                                predicted_moth.z >= near_z && predicted_moth.z <= far_z;

            if (inside || distance < 0.52f) {
                Vector3 escape = normalized3(away);
                if (length_squared3(escape) < 1.0e-5f) {
                    // The road is a solid lower boundary, so escape up or sideways.
                const float edge_left = predicted_moth.x - left;
                    const float edge_right = right - predicted_moth.x;
                    const float edge_up = top_y - predicted_moth.y;
                const float edge_near = predicted_moth.z - near_z;
                    const float edge_far = far_z - predicted_moth.z;
                const float edge = min({edge_left, edge_right, edge_up, edge_near, edge_far});
                    if (edge == edge_up) escape = {0.0f, 1.0f, 0.0f};
                    else if (edge == edge_left) escape = {-1.0f, 0.0f, 0.0f};
                    else if (edge == edge_right) escape = {1.0f, 0.0f, 0.0f};
                    else if (edge == edge_near) escape = {0.0f, 0.0f, -1.0f};
                    else escape = {0.0f, 0.0f, 1.0f};
                }
                selected_escape = escape;
                 const float bus_speed =obstacle.amplitude * obstacle.speed * cos(phase) / 55.0f;
                selected_bus_velocity =obstacle.vertical ? Vector3{0.0f, 0.0f, bus_speed}
                                                          : Vector3{bus_speed, 0.0f, 0.0f};
                predicted_contact= true;
                break;
            }
        }

        if (predicted_contact) 
        {
            const Vector3 desired_escape= add3(selected_bus_velocity, multiply3(selected_escape, maximum_speed));
            acceleration=add3(acceleration, multiply3(subtract3(desired_escape, self.velocity), avoidance_weight * 1.4f));
        }
    }

    if (self.position.x < flight_min_x + 0.4f) acceleration.x += 3.0f;
    if (self.position.x > flight_max_x - 0.4f) acceleration.x -= 3.0f;
    if (self.position.y < flight_min_height + 0.15f) acceleration.y += 3.0f;
    if (self.position.y > active_flight_ceiling() - 0.15f) acceleration.y -= 3.0f;
    if (self.position.z < flight_min_depth + 0.15f) acceleration.z += 2.5f;
    if (self.position.z > flight_max_depth - 0.15f) acceleration.z -= 2.5f;

   
   
    const float acceleration_length = length3(acceleration);
   
   
   
    constexpr float max_acceleration = 5.8f;
    if (acceleration_length > max_acceleration) acceleration = multiply3(acceleration, max_acceleration / acceleration_length);
    return acceleration;
}





void resolve_bus_overlap(agent& moth, const vector<obstacle>& moving_obstacles) {
    constexpr float moth_radius = 0.45f;
    constexpr float bus_height = 2.31f;
    for (const obstacle& bus : moving_obstacles) {
        const float center_x = (bus.bounds.x + bus.bounds.width * 0.5f - GetScreenWidth() * 0.5f) / 55.0f;
        const float center_z = (bus.bounds.y + bus.bounds.height * 0.5f - 580.0f) / 55.0f;
        const float half_x = bus.bounds.width * 1.08f / 110.0f + moth_radius;
        const float half_z = bus.bounds.height / 110.0f + moth_radius;
        const float min_x = center_x - half_x;
        const float max_x = center_x + half_x;
        const float min_y = flight_min_height;
        const float max_y = bus_height + moth_radius;
        const float min_z = center_z - half_z;
        const float max_z = center_z + half_z;

        // Temporary overlap-boundary diagnostic; this can print frequently.
        // cout << "overlap check: moth=("
        //      << moth.position.x << ", "
        //      << moth.position.y << ", "
        //      << moth.position.z << ") bus_bounds=("
        //      << min_x << ", " << max_x << ", "
        //      << min_y << ", " << max_y << ", "
        //      << min_z << ", " << max_z << ")"
        //      << endl;

        if (moth.position.x <= min_x || moth.position.x >= max_x ||
            moth.position.y < min_y || moth.position.y >= max_y ||
            moth.position.z <= min_z || moth.position.z >= max_z) {
            continue;
        }


        const float push_left = moth.position.x - min_x;
        const float push_right = max_x - moth.position.x;
        const float push_up = max_y - moth.position.y;
            const float push_near = moth.position.z - min_z;
        const float push_far = max_z - moth.position.z;
        const float smallest_push = min({push_left, push_right, push_up, push_near, push_far});
        float outward_speed = 0.0f;
        if (smallest_push == push_left) {
            moth.position.x = min_x - 0.01f;
            outward_speed = -moth.velocity.x;
            if (outward_speed < 0.35f) moth.velocity.x = -0.35f;
        } else if (smallest_push == push_right) {
            moth.position.x = max_x + 0.01f;
            outward_speed = moth.velocity.x;
            if (outward_speed < 0.35f) moth.velocity.x = 0.35f;
        } else if (smallest_push == push_up) 
        {
            moth.position.y = max_y + 0.01f;
            outward_speed = moth.velocity.y;
            if (outward_speed < 0.35f) moth.velocity.y = 0.35f;
        } else if (smallest_push == push_near) 
        {
            moth.position.z = min_z - 0.01f;
            outward_speed = -moth.velocity.z;
            if (outward_speed < 0.35f) moth.velocity.z = -0.35f;
        } else
         {
            moth.position.z = max_z + 0.01f;
            outward_speed = moth.velocity.z;
            if (outward_speed < 0.35f) moth.velocity.z = 0.35f;
        }
    }
}


void update_agents(float dt) {
    spatial_grid_instance.rebuild(agents);
    static const vector<obstacle> no_obstacles;
    vector<Vector3> accelerations(agents.size());
    // Compute every acceleration from the same pre-integration state.
    for (size_t i = 0; i < agents.size(); ++i) {
        spatial_grid_instance.query(agents[i].position, neighbor_radius, nearby_agents);
        accelerations[i] = compute_student_acceleration(i, nearby_agents, agents, lamps,
                                                         obstacles_enabled ? obstacles : no_obstacles);

        // Print one agent's acceleration while debugging, without flooding output.
        // if (i == 0) {
        //     cout << "agent 0 acceleration: "
        //          << accelerations[i].x << ", "
        //          << accelerations[i].y << ", "
        //          << accelerations[i].z
        //          << endl;
        // }
    }

    for (size_t i = 0; i < agents.size(); ++i) {
        agent& a = agents[i];
        const Vector3 acceleration = accelerations[i];
        a.velocity.x += acceleration.x * dt;

        a.velocity.y += acceleration.y * dt;
        a.velocity.z += acceleration.z * dt;

        // Limit velocity to keep the semi-implicit Euler step controlled.
        const float speed_sq= length_squared3(a.velocity);
        if (speed_sq > maximum_speed * maximum_speed) {
            const float scale = maximum_speed / sqrt(speed_sq);
            a.velocity.x *= scale;
            a.velocity.y *= scale;
            a.velocity.z *= scale;
        }
        a.position.x += a.velocity.x * dt;
        a.position.y += a.velocity.y * dt;
        a.position.z += a.velocity.z * dt;

        // Check for invalid coordinates after position integration.
        // if (!isfinite(a.position.x) ||
        //     !isfinite(a.position.y) ||
        //     !isfinite(a.position.z)) {
        //     cout << "invalid position for agent " << i
        //          << ": " << a.position.x
        //          << ", " << a.position.y
        //          << ", " << a.position.z
        //          << endl;
        // }
        a.wing_phase += dt * (18.0f + sqrt(speed_sq) * 0.08f);
        if (a.position.x < flight_min_x) a.position.x = flight_max_x;
        if (a.position.x > flight_max_x) a.position.x = flight_min_x;
        if (a.position.y < flight_min_height) {
            a.position.y = flight_min_height;

            a.velocity.y = abs(a.velocity.y) * 0.6f;
        }

        if (a.position.y > active_flight_ceiling()) {
            a.position.y = active_flight_ceiling();
            a.velocity.y = -abs(a.velocity.y) * 0.6f;
          }

        if (a.position.z < flight_min_depth) {
            a.position.z = flight_min_depth;
            a.velocity.z = abs(a.velocity.z) * 0.6f;
    }

        if (a.position.z > flight_max_depth) {
            a.position.z = flight_max_depth;
            a.velocity.z = -abs(a.velocity.z) * 0.6f;
        }

        if (obstacles_enabled) resolve_bus_overlap(a, obstacles);
    }
}



void draw_moth(const agent& a) {
    Vector3 forward = normalized3({a.velocity.x, 0.0f, a.velocity.z});
    if (length3({forward.x, 0.0f, forward.z}) < 0.01f) forward = {1.0f, 0.0f, 0.0f};
    const Vector3 center = a.position;
    const Vector3 side{forward.z, 0.0f, -forward.x};
    const float scale = a.size;
    const float flap = sin(a.wing_phase) * 0.15f;


    const Vector3 tail = {center.x - forward.x * 0.16f * scale, center.y, center.z - forward.z * 0.16f * scale};
    const Vector3 nose = {center.x + forward.x * 0.19f * scale, center.y, center.z + forward.z * 0.19f * scale};
   
   
    DrawCylinderEx(tail, nose, 0.035f * scale, 0.022f * scale, 7, a.tint);
    DrawSphere(nose, 0.043f * scale, Color{255, 225, 159, 255});
    
    
    
    const Vector3 wing_root = {center.x - forward.x * 0.015f, center.y, center.z - forward.z * 0.015f};
    for (float sign : {-1.0f, 1.0f}) {
        const Vector3 tip = {wing_root.x + side.x * sign * 0.29f * scale,
                             center.y + 0.10f * scale + flap * sign,
                             wing_root.z + side.z * sign * 0.29f * scale};
        
                             const Vector3 rear = {center.x - forward.x * 0.12f * scale + side.x * sign * 0.12f * scale,
                              center.y + 0.04f * scale,
                              center.z - forward.z * 0.12f * scale + side.z * sign * 0.12f * scale};
        DrawTriangle3D(wing_root, tip, rear, Color{211, 184, 137, 210});
        DrawLine3D(wing_root, tip, Color{244, 220, 174, 230});
    }
    const Vector3 feeler_a{nose.x +forward.x *0.09f + side.x * 0.06f,
                           nose.y + 0.035f, nose.z + forward.z * 0.09f + side.z * 0.06f};
    const Vector3 feeler_b{nose.x +forward.x *0.09f - side.x * 0.06f,
                           nose.y + 0.035f, nose.z + forward.z * 0.09f - side.z * 0.06f};
    DrawLine3D(nose, feeler_a, Color{240, 218, 175, 255});

    DrawLine3D(nose, feeler_b, Color{240, 218, 175, 255});
}



void draw_background() {
    ClearBackground(Color{9, 15, 26, 255});
    DrawPlane({0.0f, -0.22f, 3.3f}, {31.0f, 8.0f}, Color{26, 34, 39, 255});
    DrawCube({0.0f, -0.08f, 3.3f}, 29.0f, 0.12f, 4.2f, Color{39, 43, 44, 255});

    // Wet asphalt lane markings and raised sidewalks.
    for (float x = -13.0f; x < 14.0f; x += 3.0f)
        DrawCube({x, 0.0f, 3.3f}, 1.25f, 0.015f, 0.055f, Color{207, 180, 119, 180});
    DrawCube({0.0f, 0.08f, 0.65f}, 29.0f, 0.22f, 0.32f, Color{67, 68, 62, 255});
    
    DrawCube({0.0f, 0.08f, 5.95f}, 29.0f, 0.22f, 0.32f, Color{67, 68, 62, 255});
    // Animated rain in the 3D volume and thin wet-road reflections.


    for (int i = 0; i < 150; ++i) {
        const float x = fmod(i * 1.731f + simulation_time * (0.8f + (i % 5) * 0.12f), 28.0f) - 14.0f;
        const float y = 7.5f - fmod(i * 0.619f + simulation_time * (2.8f + (i % 7) * 0.18f), 7.0f);
        const float z = -5.5f + fmod(i * 0.437f, 13.0f);
        DrawLine3D({x, y, z}, {x - 0.045f, y - 0.20f, z + 0.035f}, Color{139, 180, 196, 68});
    }
}



void draw_lamps() {
    for (int i = 0; i < static_cast<int>(lamps.size()); ++i) {
        const lamp& lamp = lamps[i];
        const Vector3 base = simulation_to_world(lamp.position, 0.0f);
        const Vector3 top = simulation_to_world(lamp.position, 4.8f);
        const Vector3 arm = {top.x - 0.75f, 4.62f, top.z};
        const Vector3 bulb = simulation_to_world({lamp.position.x - 55.0f, lamp.position.y - 3.0f}, 4.48f);
        
        
        DrawCylinderEx(base, top, 0.105f, 0.065f, 10, Color{50, 59, 62, 255});
        
        
        DrawCylinderEx(top, arm, 0.065f, 0.05f, 8, Color{50, 59, 62, 255});
        
        
        DrawCylinderEx(arm, bulb, 0.05f, 0.04f, 8, Color{50, 59, 62, 255});
        
        
        DrawSphere(bulb, 0.15f, lamp.on ? Color{255, 226, 166, 255} : Color{77, 87, 91, 255});
        if (lamp.on) {
            const float pulse = 0.92f + 0.08f * sin(simulation_time * 3.1f + lamp.phase);
            DrawSphere(bulb, 0.45f * pulse, Color{255, 195, 103, 62});
            DrawSphere(bulb, 0.85f * pulse, Color{255, 179, 91, 20});
        }
        DrawCylinder({base.x, 0.07f, base.z}, 0.24f, 0.29f, 0.14f, 12, Color{69, 74, 69, 255});
    }


}



void draw_obstacles() {
    if (!obstacles_enabled) {
        return;
    }

    for (const obstacle& vehicle : obstacles) {
        const Vector2 sim_center{vehicle.bounds.x + vehicle.bounds.width * 0.5f,
                                 vehicle.bounds.y + vehicle.bounds.height * 0.5f};
        const Vector3 center = simulation_to_world(sim_center, 0.0f);


        const float length_x = vehicle.bounds.width / 55.0f;
        const float body_depth = vehicle.bounds.height / 55.0f;
        const float z = center.z;

        // Both lanes use the same tall city-bus model.
        DrawCube({center.x, 1.28f, z}, length_x * 1.08f, 1.9f, body_depth, Color{72, 119, 128, 255});
        DrawCube({center.x + length_x * 0.54f, 1.36f, z}, 0.035f, 0.55f,
                 body_depth * 0.72f, Color{123, 171, 181, 225});
        for (float x_fraction : {-0.34f, -0.11f, 0.12f, 0.35f}) {
            for (float z_sign : {-1.0f, 1.0f}) {
                DrawCube({center.x + length_x * x_fraction, 1.62f, z + z_sign * body_depth * 0.505f},
                         length_x * 0.16f, 0.62f, 0.025f, Color{38, 63, 72, 255});
        }
        }
        DrawCube({center.x + length_x * 0.53f, 1.67f, z}, 0.035f, 0.72f,
                 body_depth * 0.76f, Color{117, 165, 175, 230});
        DrawCube({center.x, 2.27f, z}, length_x * 0.92f, 0.08f, body_depth * 0.94f,
                 Color{49, 82, 91, 255});
        for (float x_offset : {-0.35f, 0.35f}) {
            for (float z_sign : {-1.0f, 1.0f}) {
                DrawCylinderEx({center.x + x_offset * length_x, 0.34f, z + z_sign * body_depth * 0.43f},
                               {center.x + x_offset * length_x, 0.34f, z + z_sign * body_depth * 0.56f},
                               0.23f, 0.23f, 10, Color{20, 22, 23, 255});
            }
        }


        DrawCube({center.x + length_x * 0.56f, 0.72f, z - body_depth * 0.28f},
                 0.05f, 0.14f, 0.18f, Color{255, 230, 174, 255});
        DrawCube({center.x + length_x * 0.56f, 0.72f, z + body_depth * 0.28f},
                 0.05f, 0.14f, 0.18f, Color{255, 230, 174, 255});


    }
}


void start_simulation() {
    desired_agent_count = clamp(draft_agent_count, 30, 1000);
    light_period = draft_light_period;
    active_mode = draft_mode;


    light_attraction_enabled = active_mode != run_mode::no_light_attraction;
    simulation_time = 0.0f;


    automatic_lights = active_mode != run_mode::manual_lights;
    reset_scene();


    paused = false;
    simulation_started = true;


    update_lamps();
    update_obstacles();
}

void handle_setup_input() {
    const float panel_x = 24.0f;
    const float panel_y = 24.0f;

    const Vector2 mouse = GetMousePosition();

    if (editing_draft_population) {
        if (IsKeyPressed(KEY_ESCAPE)) {
            editing_draft_population = false;
        }

        if (IsKeyPressed(KEY_BACKSPACE) && !draft_population_text.empty()) {
              draft_population_text.pop_back();
        }

        if (IsKeyPressed(KEY_ENTER)) {
            if (!draft_population_text.empty()) {
                draft_agent_count = clamp(stoi(draft_population_text), 30, 1000);
            }

            editing_draft_population = false;
        }

        int character = GetCharPressed();
        while (character > 0) {
            if (character >= '0' && character <= '9' && draft_population_text.size() < 4) {
                draft_population_text.push_back(static_cast<char>(character));
            }

            character = GetCharPressed();
        }

        return;
    }



    if (IsKeyPressed(KEY_N)) 
    {
        editing_draft_population = true;
        draft_population_text.clear();
    }


    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && mouse.x >= panel_x + 40.0f && mouse.x <= panel_x + 480.0f) {
        const float fraction = clamp((mouse.x - (panel_x + 40.0f)) / 440.0f, 0.0f, 1.0f);
        if (mouse.y >= panel_y + 135.0f && mouse.y <= panel_y + 155.0f) draft_agent_count = 30 + static_cast<int>(fraction * 970.0f);
        if (draft_mode != run_mode::manual_lights && mouse.y >= panel_y + 218.0f && mouse.y <= panel_y + 238.0f) draft_light_period = 10.0f + fraction * 50.0f;
    }
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        const Rectangle cycle_button{panel_x + 40.0f, panel_y + 286.0f, 140.0f, 56.0f};
         const Rectangle manual_button{panel_x + 190.0f, panel_y + 286.0f, 140.0f, 56.0f};
        
         const Rectangle flock_button{panel_x + 340.0f, panel_y + 286.0f, 140.0f, 56.0f};
        const Rectangle start_button{panel_x + 40.0f, panel_y + 370.0f, 440.0f, 62.0f};
        
         if (CheckCollisionPointRec(mouse, cycle_button)) draft_mode = run_mode::cycle_lights;
        
        if (CheckCollisionPointRec(mouse, manual_button)) draft_mode = run_mode::manual_lights;
        if (CheckCollisionPointRec(mouse, flock_button)) draft_mode = run_mode::no_light_attraction;
        if (CheckCollisionPointRec(mouse, start_button)) start_simulation();
    }
}





void handle_runtime_input() 
{
        if (IsKeyPressed(KEY_SPACE)) paused = !paused;
    if (IsKeyPressed(KEY_O)) obstacles_enabled = !obstacles_enabled;
    if (IsKeyPressed(KEY_R)) {
        simulation_started = false;
        paused = false;
            draft_agent_count = desired_agent_count;
        draft_light_period = light_period;
        draft_mode = active_mode;
    }
    const Vector2 mouse = GetMousePosition();
    if (active_mode ==run_mode::manual_lights && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        for (int i = 0; i < static_cast<int> (lamps.size()); ++i) 
        {
            const Rectangle lamp_button{37.0f, 143.0f + i * 34.0f, 360.0f, 29.0f};
            if (CheckCollisionPointRec(mouse, lamp_button)) {
                lamps[i].on = !lamps[i].on;
                return;
        }
        }
    }
}





void update_camera() {
    const Vector2 mouse = GetMousePosition();
    const bool over_hud = !simulation_started ||
                          (mouse.x < 440.0f && mouse.y < (active_mode == run_mode::manual_lights ? 310.0f : 170.0f));
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && !over_hud) {
        const Vector2 delta = GetMouseDelta();
         camera_yaw -= delta.x * 0.005f;
        camera_pitch = clamp(camera_pitch + delta.y * 0.004f, 0.10f, 1.25f);
    }


    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) 
    {
        const Vector2 delta = GetMouseDelta();
        const Vector3 forward = normalized3({camera_target.x - camera.position.x,
                                             camera_target.y - camera.position.y,
                                             camera_target.z - camera.position.z});
          
          
Vector3 right = normalized3({forward.z, 0.0f, -forward.x});
        if (length3({right.x, 0.0f, right.z}) < 0.01f) right = {1.0f, 0.0f, 0.0f};
        
        
        camera_target = add3(camera_target, multiply3(right, -delta.x * camera_distance * 0.0016f));
        camera_target.y += delta.y * camera_distance * 0.0016f;
    }




    
    
camera_distance = clamp(camera_distance * exp(-GetMouseWheelMove() * 0.10f), 13.0f, 44.0f);
    
    
    
    camera.position = {camera_target.x + camera_distance * cos(camera_pitch) * sin(camera_yaw),
                       camera_target.y + camera_distance * sin(camera_pitch),
                       camera_target.z + camera_distance * cos(camera_pitch) * cos(camera_yaw)};
    camera.target = camera_target;
    camera.up = {0.0f, 1.0f, 0.0f};
}




void draw_hud() {
    const bool manual = active_mode == run_mode::manual_lights;
    const int panel_height = manual ? 290 : 150;
    
    DrawRectangle(18, 16, 410, panel_height, Color{9, 15, 23, 224});
    
    DrawRectangle(18, 16, 4, panel_height, Color{235, 174, 92, 255});
DrawText("AFTER THE RAIN", 36, 27, 22, Color{247, 230, 194, 255});
    
const char* mode_label = active_mode == run_mode::cycle_lights ? "MODE: CYCLE LIGHTS" :
                             manual ? "MODE: MANUAL LIGHTS" : "MODE: NO LIGHT ATTRACTION";
     DrawText(mode_label, 37, 57, 12, Color{255, 207, 117, 255});
    
     DrawText(TextFormat("%d moths   |   %d/%d lamps on", static_cast<int>(agents.size()),
                        static_cast<int>(count_if(lamps.begin(), lamps.end(), [](const lamp& l) { return l.on; })),
                        static_cast<int>(lamps.size())), 37, 80, 12, RAYWHITE);
        DrawText(manual ? "drag: orbit   right drag: pan   wheel: zoom" :"drag: orbit   wheel: zoom   space: pause   R: setup",37, 105, 10, Color{182, 195, 194, 255});
    
        if (!manual) DrawText("O: toggle traffic", 37, 123, 10, Color{182, 195, 194, 255});
    
        if (manual) {
        DrawText("LAMPS", 37, 127, 10, Color{244, 220, 177, 255});
        
        
        for (int i = 0; i < static_cast<int>(lamps.size()); ++i) {
            const Rectangle button{37.0f, 143.0f + i * 34.0f, 360.0f, 29.0f};
            DrawRectangleRounded(button, 0.16f, 5, lamps[i].on ? Color{93, 81, 53, 235} : Color{40, 47, 53, 235});
        
        DrawRectangleRoundedLines(button, 0.16f, 5, lamps[i].on ? Color{255, 211, 125, 255} : Color{110, 124, 128, 255});
            DrawCircle(53, static_cast<int>(button.y + 14), 5,
                       lamps[i].on ? Color{255, 222, 148, 255} : Color{89, 98, 99, 255});
            DrawText(TextFormat("LAMP %d   %s", i + 1, lamps[i].on ? "ON" : "OFF"),
                     67, static_cast<int>(button.y + 8), 11, RAYWHITE);
        }
    }
}

void draw_setup_ui() {
      DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Color{4, 8, 14, 150});
    const float x = 24.0f;
    const float y = 24.0f;

const Rectangle panel{x, y, 540.0f, 460.0f};
    DrawRectangleRounded(panel, 0.035f, 12, Color{11, 19, 28, 248});
      
    
    DrawRectangle(static_cast<int>(x), static_cast<int>(y), 540, 5, Color{235, 174, 92, 255});
    
    
    DrawText("AFTER THE RAIN", static_cast<int>(x + 34), static_cast<int>(y + 25), 30, Color{247, 230, 194, 255});


    const char* population_value = editing_draft_population ? TextFormat(" (typing %s_)", draft_population_text.c_str()) : "";
    DrawText(TextFormat("MOTHS: %d%s", draft_agent_count, population_value),
             
    static_cast<int>(x + 40), static_cast<int>(y + 86), 14, Color{244, 220, 177, 255});
    
    
             const Rectangle population_track{x + 40.0f, y + 145.0f, 440.0f, 10.0f};
    DrawRectangleRounded(population_track, 0.5f, 6, Color{47, 62, 69, 255});
    
    
    
    
    const float population_fraction = (draft_agent_count - 30) / 970.0f;
    
    
    DrawRectangleRounded({population_track.x, population_track.y, population_track.width * population_fraction, population_track.height},
                         0.5f, 6, Color{220, 158, 76, 255});
   
   
                         DrawCircle(static_cast<int>(population_track.x + population_track.width * population_fraction),
               static_cast<int>(population_track.y + population_track.height * 0.5f), 8, Color{255, 225, 170, 255});
    DrawText("N: type count (30-1000)", static_cast<int>(x + 40),
             static_cast<int>(y + 161), 10, Color{174, 190, 190, 255});

    
    
             DrawText(draft_mode == run_mode::manual_lights ? "LIGHT CYCLE: MANUAL" :
             TextFormat("LIGHT CYCLE: %.0f s", draft_light_period),
             static_cast<int>(x + 40), static_cast<int>(y + 192), 14, Color{244, 220, 177, 255});
    
    
             const Rectangle period_track{x + 40.0f, y + 218.0f, 440.0f, 10.0f};
    DrawRectangleRounded(period_track, 0.5f, 6, draft_mode == run_mode::manual_lights ? Color{33, 40, 43, 255} : Color{47, 62, 69, 255});
    
    
        const float period_fraction = (draft_light_period - 10.0f) / 50.0f;
     DrawRectangleRounded({period_track.x, period_track.y, period_track.width * period_fraction, period_track.height},
                         0.5f, 6, draft_mode == run_mode::manual_lights ? Color{69, 72, 68, 255} : Color{220, 158, 76, 255});
                     DrawCircle(static_cast<int>(period_track.x + period_track.width * period_fraction),
               static_cast<int>(period_track.y + period_track.height * 0.5f), 8,
               draft_mode == run_mode::manual_lights ? Color{103, 105, 98, 255} : Color{255, 225, 170, 255});
    DrawText("MODE", static_cast<int>(x + 40), static_cast<int>(y + 258), 12, Color{244, 220, 177, 255});

    const Rectangle cycle_button{x + 40.0f, y + 286.0f, 140.0f, 56.0f};
    const Rectangle manual_button{x + 190.0f, y + 286.0f, 140.0f, 56.0f};
    const Rectangle flock_button{x + 340.0f, y + 286.0f, 140.0f, 56.0f};
    const auto mode_color = [](bool selected) { return selected ? Color{74, 121, 96, 255} : Color{37, 49, 55, 255}; };
    
    DrawRectangleRounded(cycle_button, 0.18f, 6, mode_color(draft_mode == run_mode::cycle_lights));
    
    DrawRectangleRounded(manual_button, 0.18f, 6, mode_color(draft_mode == run_mode::manual_lights));
    DrawRectangleRounded(flock_button, 0.18f, 6, mode_color(draft_mode == run_mode::no_light_attraction));
    DrawRectangleRoundedLines(cycle_button, 0.18f, 6, Color{162, 190, 165, 255});
    DrawRectangleRoundedLines(manual_button, 0.18f, 6, Color{162, 190, 165, 255});
    
    DrawRectangleRoundedLines(flock_button, 0.18f, 6, Color{162, 190, 165, 255});
    DrawText("CYCLE", static_cast<int>(x + 82), static_cast<int>(y + 307), 13, RAYWHITE);
   
    DrawText("MANUAL", static_cast<int>(x + 225), static_cast<int>(y + 307), 13, RAYWHITE);
    DrawText("NO ATTRACTION", static_cast<int>(x + 352), static_cast<int>(y + 307), 11, RAYWHITE);

    const Rectangle start_button{x + 40.0f, y + 370.0f, 440.0f, 62.0f};
    DrawRectangleRounded(start_button, 0.16f, 8, Color{196, 131, 60, 255});
    DrawRectangleRoundedLines(start_button, 0.16f, 8, Color{255, 220, 155, 255});
    DrawText("START", static_cast<int>(x + 226), static_cast<int>(y + 390), 19, RAYWHITE);
}

} // namespace

int main() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
     
     InitWindow(screen_width, screen_height, "After the Rain - Streetlight Swarm");
      SetWindowMinSize (1050, 700);
    SetTargetFPS(60);

     initialize_scene();
    camera = {};
    camera.up = {0.0f, 1.0f, 0.0f};
    camera.fovy = 45.0f;

    camera.projection = CAMERA_PERSPECTIVE;

        update_camera();

    while (!WindowShouldClose()) {
        const float dt = min(GetFrameTime(), 0.025f);

        if (simulation_started) handle_runtime_input();


        else handle_setup_input();
        update_camera();
        if (simulation_started && !paused) {
            simulation_time += dt;

            // Print one status line per second while debugging.
            // static int debug_frame = 0;
            // if (debug_frame++ % 60 == 0) {
            //     cout << "time=" << simulation_time
            //          << " agents=" << agents.size()
            //          << " obstacles=" << obstacles.size()
            //          << endl;
            // }


            update_lamps();
            update_obstacles();
            update_agents(dt);
        }

        BeginDrawing();
        BeginMode3D(camera);


        draw_background();
        draw_obstacles();


        draw_lamps();


        for (const agent& a : agents) draw_moth(a);
        EndMode3D();


        if (simulation_started) {
            draw_hud();
            if (paused) DrawText("PAUSED", GetScreenWidth() - 125, 220, 20, Color{173, 226, 220, 255});
        } else {
            draw_setup_ui();
        }
        EndDrawing();


    }
    CloseWindow();


    return 0;
}
