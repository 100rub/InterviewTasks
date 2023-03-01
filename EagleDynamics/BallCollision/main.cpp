// Apologies to whoever will have to check this.
// 
// This code is far from properly optimized, but I've spent far too 
// much time researching and agonizing over selecting optimal spatial indexing solution.
// 
// In the end I did not have enough time to implement (and debug) any kind of tree-based partitioning
// so I went with spacial hashing approach, which should be good enough, even if its definitely not the best.
// 
// Techically speaking, complexity of this approach still seems to be O(n^2 / k) 
// even if k is quite big, and we can make it even bigger by adjusting grid size.
// 
// With further and further optimizations it becomes quite hard to gauge performance, 
// because collision code already takes about 5% of the cycle, with good 90% taken by SFML render.
// 

#include "SFML/Graphics.hpp"
#include "MiddleAverageFilter.h"

#include <iostream>
#include <unordered_map>
#include <algorithm>
#include <chrono>

using cellID = std::pair<uint16_t, uint16_t>;

constexpr int WINDOW_X = 1024;
constexpr int WINDOW_Y = 768;
constexpr int MAX_BALLS = 300;
constexpr int MIN_BALLS = 100;

// these values are based on the very rough principle of making a grid cell about twice as big as the biggest object
// unfortunately, if ball sizes change, it will have to be adjusted
size_t pX = 16;
size_t pY = 16;

// these are counters to empyrically estimate algorithm complexity
//size_t complexity_counter = 0;
//size_t min_complexity = 0-1;
//size_t max_complexity = 0;

Math::MiddleAverageFilter<float,100> fpscounter;
//Math::MiddleAverageFilter<size_t, 1000> bench;

// we have to specialize std::hash and implement a basic hash function for std::pair to be able to use it as a key for unordered_map
template<>
struct std::hash<cellID>
{
    std::size_t operator()(cellID const& s) const noexcept
    {
        std::size_t h1 = std::hash<uint16_t>{}(s.first);
        std::size_t h2 = std::hash<uint16_t>{}(s.second);
        return h1 ^ (h2 << 1);
    }
};

// probably worth it to convert to class and make a proper interface
struct Ball
{
    /// ball position on screen
    sf::Vector2f pos;

    /// ball movement vector
    /// ball speed is equal its magnitude
    sf::Vector2f dir;

    // ball radius
    float rad = 0;

    // graphical representation of the ball on screen
    sf::CircleShape gball;
    /// we save it inside the struct to avoid re-creating it every frame
};

// so we dont have to bother with pow() everywhere
template <class TValue>
static inline auto sqr(TValue value)
{
    return value * value;
}

auto& cell_map()
{
    // we use a hash-based map store pointers to our balls
    // this is better than storing balls directly, since they would have to be moved around between buckets, and we can just move pointers instead
    // this way we can keep the balls themselves in the (essentially) runtime-immutable vector and use it for render loops
    static auto m = std::unordered_map<cellID, std::vector<Ball*>>();
    return m;
}

// moving edge collision check into a separate function for convenience later
template <bool top, bool bot, bool left, bool right>
static inline void edge_collision_check(uint16_t x, uint16_t y)
{
    cellID cur_id(x, y);
    auto& balls = cell_map()[cur_id];

    for (auto& ball : balls)
    {
        if constexpr (top)
            if (ball->pos.y <= ball->rad && ball->dir.y < 0)
                ball->dir.y = -ball->dir.y;

        if constexpr (bot)
            if (ball->pos.y >= (WINDOW_Y - ball->rad) && ball->dir.y > 0)
                ball->dir.y = -ball->dir.y;

        if constexpr (left)
            if (ball->pos.x <= ball->rad && ball->dir.x < 0)
                ball->dir.x = -ball->dir.x;

        if constexpr (right)
            if (ball->pos.x >= (WINDOW_X - ball->rad) && ball->dir.x > 0)
                ball->dir.x = -ball->dir.x;
    }
};

// our "hash" function to determine which cell the ball belongs to
cellID get_cell_id(const Ball& ball)
{
    cellID res;

    float cell_width = (float)WINDOW_X / pX;
    float cell_height = (float)WINDOW_Y / pY;

    res.first = std::floor(ball.pos.x / cell_width);
    res.second = std::floor(ball.pos.y / cell_height);

    return res;
}

void move_ball(Ball& ball, float deltaTime)
{
    // we store before-movement cell id to compare it later
    auto old_cell_id = get_cell_id(ball);

    float dx = ball.dir.x * deltaTime;
    float dy = ball.dir.y * deltaTime;
    ball.pos.x += dx;
    ball.pos.y += dy;

    auto new_cell_id = get_cell_id(ball);

    // if the ball have moved to another cell, we need to update the map
    if (old_cell_id != new_cell_id)
    {
        auto& old_cell = cell_map()[old_cell_id];
        old_cell.erase(std::find(old_cell.begin(), old_cell.end(), &ball));
        cell_map()[new_cell_id].push_back(&ball);
    }

    // origin point for all sfml shapes is in top-left corner, which is aids to work with in case of circles
    // we adjust graphic representation of the circle so its logical coordinates make sense in reference to window
    // this way a circle whose position is (0,0) is actually in top-left possible position on the screen
    ball.gball.setPosition(ball.pos.x - ball.rad, ball.pos.y - ball.rad);
}

// we only want to check against specific nearby cells in the following pattern
// 0 | 0 | 0
// 0 | c | х
// х | х | х
// where 
// c - current cell
// x - cell we want to check against
// this is so we dont double-check any cell pair against each other
static inline std::vector<cellID> get_adjacent_cell_ids(const cellID& id)
{
    std::vector<cellID> res;
    res.reserve(4);

    // if its the bottom-right cell
    if (id.first == pX - 1 && id.second == pY - 1)
        return res;
    
    // right cell
    if (id.first < pX - 1)
        res.emplace_back(id.first + 1, id.second);

    // bottom cells
    if (id.second < pY - 1)
    {
        if (id.first > 0)
            res.emplace_back(id.first - 1, id.second + 1);

        res.emplace_back(id.first, id.second + 1);

        //bottom right cell
        if (id.first < pX - 1)
            res.emplace_back(id.first + 1, id.second + 1);
    }

    return res;
}

void collide_balls(Ball& ball1, Ball& ball2)
{
    // we'll assume our circles are in fact spheres and calculate mass accordingly
    double m1 = 3.14f * sqr(ball1.rad);
    double m2 = 3.14f * sqr(ball2.rad);

    double m21 = m2 / m1;
    double x21 = ball2.pos.x - ball1.pos.x;
    double y21 = ball2.pos.y - ball1.pos.y;
    double vx21 = ball2.dir.x - ball1.dir.x;
    double vy21 = ball2.dir.y - ball1.dir.y;

    double vx_cm = (m1 * ball1.dir.x + m2 * ball2.dir.x) / (m1 + m2);
    double vy_cm = (m1 * ball1.dir.y + m2 * ball2.dir.y) / (m1 + m2);

    // do nothing if balls are not approaching
    if ((vx21 * x21 + vy21 * y21) >= 0)
        return;

    double fy21 = 1.0E-12 * fabs(y21);

    if (fabs(x21) < fy21)
    {
        if (x21 < 0)
        {
            x21 = -fy21;
        }
        else
        {
            x21 = fy21;
        }
    }

    // update velocities
    double a = y21 / x21;
    double dvx2 = -2 * (vx21 + a * vy21) / ((1 + a * a) * (1 + m21));
    ball2.dir.x = ball2.dir.x + dvx2;
    ball2.dir.y = ball2.dir.y + a * dvx2;
    ball1.dir.x = ball1.dir.x - m21 * dvx2;
    ball1.dir.y = ball1.dir.y - a * m21 * dvx2;
}

void narrow_collision(Ball& ball1, Ball& ball2)
{
    //complexity_counter++;

    // additional elimination checks
    if (abs(ball1.pos.x - ball2.pos.x) > (ball1.rad + ball2.rad))
        return;
    if (abs(ball1.pos.y - ball2.pos.y) > (ball1.rad + ball2.rad))
        return;

    // if balls are close enough, we check if they actually collide
    if (sqrt(sqr(ball1.pos.x - ball2.pos.x) + sqr(ball1.pos.y - ball2.pos.y)) <= (ball1.rad + ball2.rad))
    {
        collide_balls(ball1, ball2);
    }
}

void spatial_hashing_collision()
{
    //auto t1 = std::chrono::high_resolution_clock::now();

    // checking edge collision for corner cells
    edge_collision_check<true, false, true, false>(0, 0);
    edge_collision_check<false, true, true, false>(0, pY - 1);
    edge_collision_check<true, false, false, true>(pX - 1, 0);
    edge_collision_check<false, true, false, true>(pX - 1, pY - 1);

    // checking edge collision for cells along the top and bottom edges
    for (int x = 1; x < pX - 1; ++x)
    {
        edge_collision_check<true, false, false, false>(x, 0);
        edge_collision_check<false, true, false, false>(x, pY - 1);
    }

    // checking edge collision for cells along the left and right edges
    for (int y = 1; y < pY - 1; ++y)
    {
        edge_collision_check<false, false, true, false>(0, y);
        edge_collision_check<false, false, false, true>(pX - 1, y);
    }

    // checking for inter-ball collisions in each grid cell
    for (auto& pair : cell_map())
    {
        const auto& cur_id = pair.first;
        auto& balls = pair.second;

        // nothing to do if there are no balls
        if (balls.empty())
            continue;

        // now we check for collision within the current cell
        for (int a = 0; a < balls.size(); ++a)
        {
            for (int b = a + 1; b < balls.size(); ++b)
            {
                narrow_collision(*balls[a], *balls[b]);
            }
        }

        // next we want to check collision of each ball in current cell against each ball in some of the nearby cells
        std::vector<cellID> cell_ids = get_adjacent_cell_ids(cur_id);

        for (auto& id : cell_ids)
        {
            auto& other_balls = cell_map()[id];

            // for each ball in the current cell
            for (auto& ball1 : balls)
            {
                // and for each ball in the other cell
                for (auto& ball2 : other_balls)
                {
                    narrow_collision(*ball1, *ball2);
                }
            }
        }
    }

    //auto t2 = std::chrono::high_resolution_clock::now();

    //auto time = std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count();
    //std::cout << time << std::endl;

    //bench.push(time);
}

void naive_collision(std::vector<std::unique_ptr<Ball>>& balls)
{
    for (int a = 0; a < balls.size(); ++a)
    {
        if (balls[a]->pos.x <= balls[a]->rad || balls[a]->pos.x >= (WINDOW_X - balls[a]->rad))
            balls[a]->dir.x = -balls[a]->dir.x;

        if (balls[a]->pos.y <= balls[a]->rad || balls[a]->pos.y >= (WINDOW_Y - balls[a]->rad))
            balls[a]->dir.y = -balls[a]->dir.y;

        for (int b = a + 1; b < balls.size(); ++b)
        {
            //complexity_counter++;
            narrow_collision(*balls[a], *balls[b]);
        }
    }
}

void draw_fps(sf::RenderWindow& window, float fps)
{
    char c[32];
    snprintf(c, 32, "FPS: %f", fps);
    std::string string(c);
    sf::String str(c);
    window.setTitle(str);
}

int main()
{
    sf::RenderWindow window(sf::VideoMode(WINDOW_X, WINDOW_Y), "ball collision demo");

    srand(time(NULL));

    // this is a slower approach than storing balls directly, but it prevents pointers from being garbled if the collection is modified
    std::vector<std::unique_ptr<Ball>> balls;

    auto ball_num = (rand() % (MAX_BALLS - MIN_BALLS) + MIN_BALLS);

    // randomly initialize balls
    for (int i = 0; i < ball_num; i++)
    {
        auto newBall_ptr = std::make_unique<Ball>();

        newBall_ptr->rad = 5 + rand() % 5;

        newBall_ptr->pos.x = newBall_ptr->rad + rand() % (WINDOW_X - (int)ceil(newBall_ptr->rad * 2));
        newBall_ptr->pos.y = newBall_ptr->rad + rand() % (WINDOW_Y - (int)ceil(newBall_ptr->rad * 2));

        newBall_ptr->dir.x = (-5 + (rand() % 10)) / 3.;
        newBall_ptr->dir.y = (-5 + (rand() % 10)) / 3.;
        
        newBall_ptr->gball.setRadius(newBall_ptr->rad);
        
        // we generate speed separately and set the magnitude of dir vector accordingly
        float spd = 30 + rand() % 30;
        // this is just legacy to keep the original speed distribution range
        auto old_mag = sqrt(sqr(newBall_ptr->dir.x) + sqr(newBall_ptr->dir.y));
        newBall_ptr->dir.x = newBall_ptr->dir.x * (spd / old_mag);
        newBall_ptr->dir.y = newBall_ptr->dir.y * (spd / old_mag);

        balls.push_back(std::move(newBall_ptr));

        auto& tmp = balls.back();
        cell_map()[get_cell_id(*tmp)].push_back(tmp.get());
    }

    // window.setFramerateLimit(60);

    sf::Clock clock;
    float lastime = clock.restart().asSeconds();

    while (window.isOpen())
    {
        sf::Event event;
        while (window.pollEvent(event))
        {
            if (event.type == sf::Event::Closed)
            {
                window.close();
            }
        }

        float current_time = clock.getElapsedTime().asSeconds();
        float deltaTime = current_time - lastime;
        fpscounter.push(1.0f / (current_time - lastime));
        lastime = current_time;

        /// объекты создаются в случайном месте на плоскости со случайным вектором скорости, имеют радиус R
        /// Объекты движутся кинетически. Пространство ограниченно границами окна
        /// Напишите обработчик столкновений шаров между собой и краями окна. Как это сделать эффективно?
        /// Массы пропорцианальны площадям кругов, описывающих объекты 
        /// Как можно было-бы улучшить текущую архитектуру кода?
        /// Данный код является макетом, вы можете его модифицировать по своему усмотрению
        
        //complexity_counter = 0;

        //naive_collision(balls);
        spatial_hashing_collision();

        for (auto& ball : balls)
            move_ball(*ball, deltaTime);

        //if (complexity_counter > max_complexity)
        //    max_complexity = complexity_counter;

        //if (complexity_counter < min_complexity)
        //    min_complexity = complexity_counter;

        window.clear();

        // drawing balls
        for (const auto& ball : balls)
        {
            // this is very unoptimized, but it seems without manually screwing around with vertex arrays you cant do much here
            window.draw(ball->gball);
        }

        // drawing vertical grid lines for debug
        /*for (int i = 1; i <= pX; ++i)
        {
            sf::RectangleShape line;
            line.setSize({ 1, WINDOW_Y });
            int pos = i * ((float)WINDOW_X / pX);
            line.setPosition(pos, 0);
            window.draw(line);
        }*/

        // drawing horizontal grid lines for debug
        /*for (int i = 1; i <= pY; ++i)
        {
            sf::RectangleShape line;
            line.setSize({ WINDOW_X, 1 });
            int pos = i * ((float)WINDOW_Y / pY);
            line.setPosition(0, pos);
            window.draw(line);
        }*/

		draw_fps(window, fpscounter.getAverage());
		window.display();
    }

    //std::cout << "complexity = " << min_complexity << " - " << max_complexity << std::endl;
    //std::cout << "bench time = " << bench.getAverage() << "ns" << std::endl;
    //std::cin.get();
    return 0;
}
