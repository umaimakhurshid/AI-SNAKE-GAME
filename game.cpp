#include "C:/raylib/raylib/src/raylib.h"
#include <iostream>
#include <deque>
#include <vector>
#include <queue>
#include <algorithm>
#include <unordered_set>
#include <raymath.h>
#include <cfloat>
#include <string>
#include <fstream>
using namespace std;

const int cellsize = 30;
const int numberofcells = 25;
const int offset = 75;

Color green = {173, 204, 96, 255};
Color darkgreen = {0, 100, 0, 255};

bool menuScreen = true;
bool gameOverScreen = false;
double lastupdatetime = 0;
bool aiMode = false;

int growthFoodCount = 0;
int shrinkFoodCount = 0;
int negativeFoodCount = 0;
int highScore = 0;

int loadHighScore(const string& filename){
    ifstream file(filename);
    int score = 0;
    if(file.is_open()){
        file>>score;
        file.close();
    }
    return score;
}
void saveHighScore(const string& filename, int score){
    ofstream file(filename);
    if(file.is_open()){
        file<<score;
        file.close();
    }
}
bool ElementInDeque(Vector2 val, const deque<Vector2> &dq)
{
    for (const auto &el : dq)
        if (Vector2Equals(val, el))
            return true;
    return false;
}

bool eventtriggered(double interval)
{
    double currenttime = GetTime();
    if (currenttime - lastupdatetime >= interval)
    {
        lastupdatetime = currenttime;
        return true;
    }
    return false;
}

struct Node
{
    Vector2 position;
    float gCost = 0, hCost = 0, fCost = 0;
    Node *parent = nullptr;
};

float Heuristic(Vector2 a, Vector2 b)
{
    return abs(a.x - b.x) + abs(a.y - b.y);
}

bool IsCellWalkable(Vector2 cell, const deque<Vector2> &snake)
{
    if (cell.x < 0 || cell.x >= numberofcells || cell.y < 0 || cell.y >= numberofcells)
        return false;
    return !ElementInDeque(cell, snake);
}

vector<Vector2> AStar(Vector2 start, Vector2 goal, const deque<Vector2> &snake)
{
    vector<Vector2> path;
    auto cmp = [](Node *a, Node *b)
    { return a->fCost > b->fCost; };
    priority_queue<Node *, vector<Node *>, decltype(cmp)> openSet(cmp);
    unordered_set<string> closed;
    vector<Node *> allNodes;

    auto vec2str = [](Vector2 v)
    {
        return to_string((int)v.x) + "," + to_string((int)v.y);
    };

    Node *startNode = new Node{start};
    startNode->hCost = Heuristic(start, goal);
    startNode->fCost = startNode->hCost;
    openSet.push(startNode);
    allNodes.push_back(startNode);

    vector<Vector2> directions = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};

    while (!openSet.empty())
    {
        Node *current = openSet.top();
        openSet.pop();

        if (Vector2Equals(current->position, goal))
        {
            while (current)
            {
                path.push_back(current->position);
                current = current->parent;
            }
            reverse(path.begin(), path.end());
            break;
        }

        closed.insert(vec2str(current->position));

        for (Vector2 dir : directions)
        {
            Vector2 neighbor = Vector2Add(current->position, dir);
            if (!IsCellWalkable(neighbor, snake) || closed.count(vec2str(neighbor)))
                continue;

            float g = current->gCost + 1;
            float h = Heuristic(neighbor, goal);
            Node *next = new Node{neighbor, g, h, g + h, current};
            openSet.push(next);
            allNodes.push_back(next);
        }
    }

    for (auto n : allNodes)
        delete n;
    return path;
}

class Snake
{
public:
    deque<Vector2> body = {{4, 23}, {3, 23}, {2, 23}};
    Vector2 direction = {1, 0};
    bool addbody = false;

    void Reset()
    {
        body = {{4, 23}, {3, 23}, {2, 23}};
        direction = {1, 0};
    }

    void Draw()
    {
        for (auto &segment : body)
        {
            Rectangle r = {offset + segment.x * cellsize, offset + segment.y * cellsize, cellsize, cellsize};
            DrawRectangleRounded(r, 0.5, 8, darkgreen);
        }
    }

    void Update()
    {
        body.push_front(Vector2Add(body[0], direction));
        if (!addbody)
            body.pop_back();
        else
            addbody = false;
    }
};

class Food
{
public:
    Vector2 position;
    Texture2D texture;

    Food() {}
    Food(deque<Vector2> &snake, const char *filename)
    {
        Image img = LoadImage(filename);
        texture = LoadTextureFromImage(img);
        UnloadImage(img);
        position = GenerateRandomPos(snake);
    }

    virtual ~Food() { UnloadTexture(texture); }

    Vector2 GenerateRandomPos(deque<Vector2> &snake)
    {
        Vector2 pos;
        do
        {
            pos = {(float)GetRandomValue(0, numberofcells - 1), (float)GetRandomValue(0, numberofcells - 1)};
        } while (ElementInDeque(pos, snake));
        return pos;
    }

    virtual void Draw()
    {
        DrawTexture(texture, offset + position.x * cellsize, offset + position.y * cellsize, WHITE);
    }
};

class GrowthFood : public Food
{
public:
    GrowthFood(deque<Vector2> &snake) : Food(snake, "Graphics/growth.png") {}
    void Apply(Snake &s) { s.addbody = true; }
};

class ShrinkFood : public Food
{
public:
    ShrinkFood(deque<Vector2> &snake) : Food(snake, "Graphics/shrink.png") {}
    void Apply(Snake &s)
    {
        if (s.body.size() > 1)
            s.body.pop_back();
        else
            s.Reset();
    }
};

class NegativeFood : public Food
{
public:
    NegativeFood(deque<Vector2> &snake) : Food(snake, "Graphics/negative.png") {}
};

class Game
{
public:
    Snake snake;
    Food food;
    GrowthFood growth;
    ShrinkFood shrink;
    NegativeFood negative1, negative2;
    int score = 0;
    bool running = true;
    Sound eat, hit;

    Game() : food(snake.body, "Graphics/food.png"),
             growth(snake.body),
             shrink(snake.body),
             negative1(snake.body),
             negative2(snake.body)
    {
        InitAudioDevice();
        eat = LoadSound("Sounds/eat.mp3");
        hit = LoadSound("Sounds/wall.mp3");
    }

    ~Game()
    {
        UnloadSound(eat);
        UnloadSound(hit);
        CloseAudioDevice();
    }

    void Draw()
    {
        food.Draw();
        growth.Draw();
        shrink.Draw();
        negative1.Draw();
        negative2.Draw();
        snake.Draw();
    }

    void Update()
    {
        if (!running)
            return;

        snake.Update();
        Vector2 head = snake.body[0];

        if (Vector2Equals(head, food.position))
        {
            food.position = food.GenerateRandomPos(snake.body);
            snake.addbody = true;
            score++;
            growthFoodCount++; // increment on black food
            PlaySound(eat);
        }

        if (Vector2Equals(head, growth.position))
        {
            growth.Apply(snake);
            growth.position = growth.GenerateRandomPos(snake.body);
            score += 2;
            growthFoodCount++; // increment on blue food
            PlaySound(eat);
        }

        if (Vector2Equals(head, shrink.position))
        {
            if (snake.body.size() <= 1)
            {
                running = false;
                gameOverScreen = true;
                PlaySound(hit);
                return;
            }
            shrink.Apply(snake);
            shrink.position = shrink.GenerateRandomPos(snake.body);
            shrinkFoodCount++;
            PlaySound(hit);
        }

        if (Vector2Equals(head, negative1.position) || Vector2Equals(head, negative2.position))
        {
            if (Vector2Equals(head, negative1.position))
                negative1.position = negative1.GenerateRandomPos(snake.body);
            else
                negative2.position = negative2.GenerateRandomPos(snake.body);
            if (score == 0)
            {
                running = false;
                gameOverScreen = true;
                PlaySound(hit);
                return;
            }
            score--;
            negativeFoodCount++;
            PlaySound(hit);
        }

        if (head.x < 0 || head.y < 0 || head.x >= numberofcells || head.y >= numberofcells ||
            ElementInDeque(head, deque<Vector2>(snake.body.begin() + 1, snake.body.end())))
        {
            if(score > highScore){
                highScore = score;
                saveHighScore("highscore.txt", highScore);
            }
            running = false;
            gameOverScreen = true;
            PlaySound(hit);
        }
    }

    void Restart()
    {
        score = 0;
        growthFoodCount = shrinkFoodCount = negativeFoodCount = 0;
        snake.Reset();
        food.position = food.GenerateRandomPos(snake.body);
        growth.position = growth.GenerateRandomPos(snake.body);
        shrink.position = shrink.GenerateRandomPos(snake.body);
        negative1.position = negative1.GenerateRandomPos(snake.body);
        negative2.position = negative2.GenerateRandomPos(snake.body);
        running = true;
        gameOverScreen = false;
    }
};

int main()
{
    InitWindow(2 * offset + cellsize * numberofcells, 2 * offset + cellsize * numberofcells, "Snake AI Game");
    SetTargetFPS(60);
    highScore = loadHighScore("highscore.txt");
    Game game;

    while (!WindowShouldClose())
    {
        BeginDrawing();
        ClearBackground(green);

        if (menuScreen)
        {
            DrawText("Welcome to AI Snake Game", offset, offset - 40, 30, DARKGREEN);
            DrawText("Rules:", offset, offset, 25, BLACK);
            DrawText("- Red: -1 point, no size change", offset, offset + 30, 20, RED);
            DrawText("- Orange: no score change, -1 size", offset, offset + 60, 20, ORANGE);
            DrawText("- Blue: +2 points, +1 size", offset, offset + 90, 20, BLUE);
            DrawText("- Black: +1 point, +1 size", offset, offset + 120, 20, BLACK);
            DrawText("Press ENTER to Start", offset, offset + 180, 25, DARKGRAY);
            if (IsKeyPressed(KEY_ENTER))
                menuScreen = false;
        }
        else if (gameOverScreen)
        {
            DrawText("Game Over!", (GetScreenWidth() - MeasureText("Game Over!", 40)) / 2, offset + 100, 40, RED);
            DrawText(TextFormat("Total Score: %i", game.score),
            (GetScreenWidth() - MeasureText(TextFormat("Total Score: %i", game.score), 25)) / 2, offset + 150, 25, DARKGRAY);
            DrawText(TextFormat("High Score: %i", highScore),
            (GetScreenWidth() - MeasureText(TextFormat("High Score: %i", highScore), 25)) / 2,
            offset + 180, 25, DARKGREEN);
            DrawText("Press R to Restart or ESC to Exit",
            (GetScreenWidth() - MeasureText("Press R to Restart or ESC to Exit", 25)) / 2, offset + 230, 25, DARKGRAY);
            if (IsKeyPressed(KEY_R))
                game.Restart();
        }
        else
        {
            if (eventtriggered(0.2))
            {
                if (aiMode)
                {
                    vector<Vector2> foodTargets = {
                        game.food.position,
                        game.growth.position,
                        game.shrink.position,
                        game.negative1.position,
                        game.negative2.position};

                    // Optionally filter by priority or penalty
                    float minDist = FLT_MAX;
                    Vector2 target = game.food.position;

                    for (auto &f : foodTargets)
                    {
                        float dist = Heuristic(game.snake.body[0], f);
                        if (dist < minDist)
                        {
                            minDist = dist;
                            target = f;
                        }
                    }

                    vector<Vector2> path = AStar(game.snake.body[0], target, game.snake.body);
                    if (path.size() > 1)
                    {
                        Vector2 step = Vector2Subtract(path[1], game.snake.body[0]);
                        game.snake.direction = step;
                    }
                }
                game.Update();
            }

            if (!aiMode)
            {
                if (IsKeyPressed(KEY_UP) && game.snake.direction.y != 1)
                    game.snake.direction = {0, -1};
                if (IsKeyPressed(KEY_DOWN) && game.snake.direction.y != -1)
                    game.snake.direction = {0, 1};
                if (IsKeyPressed(KEY_LEFT) && game.snake.direction.x != 1)
                    game.snake.direction = {-1, 0};
                if (IsKeyPressed(KEY_RIGHT) && game.snake.direction.x != -1)
                    game.snake.direction = {1, 0};
            }

            if (IsKeyPressed(KEY_SPACE))
                aiMode = !aiMode;

            DrawRectangleLinesEx(Rectangle{(float)offset - 5, (float)offset - 5, numberofcells * cellsize + 10, numberofcells * cellsize + 10}, 5, darkgreen);
            DrawText(TextFormat("Score: %i", game.score), offset, offset + numberofcells * cellsize + 10, 25, BLACK);
            DrawText(TextFormat("Growth: %i | Shrink: %i | Negative: %i", growthFoodCount, shrinkFoodCount, negativeFoodCount),
                     offset + 250, offset + numberofcells * cellsize + 10, 20, DARKGRAY);
            DrawText(aiMode ? "Mode: AI (SPACE to switch)" : "Mode: Manual (SPACE to switch)", offset + 300, 20, 20, DARKGRAY);
            game.Draw();
        }

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
