#include "gamecontroller.h"
#include <QRandomGenerator>

GameController::GameController(AppController *flow, QObject *parent)
    : QObject(parent), m_flow(flow), m_model(new PuzzleModel(this))
{
    setObjectName(QStringLiteral("gameController"));
    m_timer.setSingleShot(true);
    m_timer.setInterval(0);
    connect(&m_timer, &QTimer::timeout, this, &GameController::prepare);
    connect(m_model, &PuzzleModel::completed, m_flow, &AppController::finishGame);
}

PuzzleModel *GameController::model() { return m_model; }

bool GameController::start(const QImage &image, int count, quint32 seed)
{
    if (!m_flow->beginPreparation())
        return false;
    m_pendingImage = image;
    m_pendingCount = count;
    m_pendingSeed = seed;
    m_reuse = false;
    m_timer.start();
    return true;
}

void GameController::cancel()
{
    m_timer.stop();
    m_pendingImage = {};
    m_flow->returnToSetup();
}

void GameController::restart()
{
    if (!m_flow->restartPreparation())
        return;
    m_reuse = true;
    m_pendingSeed = QRandomGenerator::global()->generate();
    m_timer.start();
}

void GameController::regenerate()
{
    if (!m_flow->restartPreparation())
        return;
    m_pendingImage = m_model->image();
    m_pendingCount = int(m_model->pieces().size());
    m_pendingSeed = QRandomGenerator::global()->generate();
    m_reuse = false;
    if (m_pendingSeed == m_model->seed())
        m_pendingSeed ^= 1u;
    m_timer.start();
}

void GameController::prepare()
{
    if (m_flow->state() != AppController::State::Preparing)
        return;
    QString error;
    // Новое изображение передаётся модели целиком, но пиксели не копируются благодаря QImage.
    if (m_reuse) {
        m_model->restart(m_pendingSeed);
    } else if (!m_model->start(m_pendingImage, m_pendingCount, m_pendingSeed, &error)) {
        cancel();
        emit preparationFailed(error);
        return;
    }
    m_pendingImage = {};
    m_flow->finishPreparation();
}

void GameController::dropPiece(int id, QPointF position, qreal tolerance)
{
    if (m_flow->state() == AppController::State::Playing)
        m_model->place(id, position, tolerance);
}

void GameController::rotatePiece(int id, int quarterTurns)
{
    if (m_flow->state() == AppController::State::Playing)
        m_model->rotate(id, quarterTurns);
}
