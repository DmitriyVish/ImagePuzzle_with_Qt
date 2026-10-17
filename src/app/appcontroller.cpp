#include "appcontroller.h"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcAppFlow, "imagepuzzle.app")

AppController::AppController(QObject *parent) : QObject(parent) {}

AppController::State AppController::state() const
{
    return m_state;
}

bool AppController::beginPreparation()
{
    return m_state == State::Configuring && transitionTo(State::Preparing);
}
bool AppController::restartPreparation()
{
    return (m_state == State::Playing || m_state == State::Completed) && transitionTo(State::Preparing);
}
bool AppController::finishPreparation() { return transitionTo(State::Playing); }
bool AppController::finishGame() { return transitionTo(State::Completed); }
bool AppController::returnToSetup() { return transitionTo(State::Configuring); }

bool AppController::transitionTo(State next)
{
    if (next == m_state)
        return false;

    // Возврат разрешён даже во время подготовки; пропуск этапов запрещён.
    bool allowed = next == State::Configuring;
    switch (m_state) {
    case State::Configuring:
        allowed = next == State::Preparing;
        break;
    case State::Preparing:
        allowed = allowed || next == State::Playing;
        break;
    case State::Playing:
        allowed = allowed || next == State::Completed || next == State::Preparing;
        break;
    case State::Completed:
        allowed = allowed || next == State::Preparing;
        break;
    }
    if (!allowed) {
        qCWarning(lcAppFlow) << "Rejected application transition:" << m_state << "->" << next;
        return false;
    }

    const State previous = m_state;
    m_state = next;
    qCInfo(lcAppFlow) << "Application state:" << previous << "->" << m_state;
    emit stateChanged(m_state);
    return true;
}
