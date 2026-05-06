#ifndef SINGLETON_H
#define SINGLETON_H

#include <QObject>
#include <QMutex>
#include <QAtomicPointer>

template <typename T>
class Singleton
{
public:
    static T* getInstance() {
        if (!m_instance) {
            QMutexLocker locker(&m_mutex);
            if (!m_instance) {
                m_instance = new T();
            }
        }
        return m_instance;
    }

    static void destroyInstance() {
        if (m_instance) {
            delete m_instance;
            m_instance = nullptr;
        }
    }

protected:
    Singleton() = default;
    virtual ~Singleton() = default;

    // 删除拷贝构造函数和赋值运算符
    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton&) = delete;

private:
    static QMutex m_mutex;
    static QAtomicPointer<T> m_instance;
};

// 静态成员初始化
template <typename T> QMutex Singleton<T>::m_mutex;
template <typename T> QAtomicPointer<T> Singleton<T>::m_instance = nullptr;

#endif // SINGLETON_H
