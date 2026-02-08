export module Chess.UCI.Signal;

import std;

import Chess.Assert;
import Chess.Position;
import Chess.Position.RepetitionMap;
import Chess.SafeInt;

import Chess.UCI.UciCommand;

namespace chess {
	export class Signal {
	private:
		mutable std::mutex m_mutex;
		std::condition_variable_any m_cv;
		bool m_shuttingDown = false;
		bool m_searchFinished = true;
		std::atomic_bool m_stopRequested = false; //read by helper threads (don't want to lock mutex on every read)
		std::list<UciCommand> m_commands;
	public:
		void setCommand(UciCommand command) {
			if (command.getType() == UciCommandType::Stop) {
				cancel();
			} else {
				{
					std::scoped_lock l{ m_mutex };
					m_commands.push_back(std::move(command));
				}
				m_cv.notify_one(); //notify thread waiting for command
			}
		}

		bool isStopRequested() const {
			return m_stopRequested.load();
		}
		
		UciCommand waitForUciCommand(std::stop_token stopToken) {
			std::unique_lock l{ m_mutex };
			m_cv.wait(l, stopToken, [this] {
				return !m_commands.empty();
			});
			if (stopToken.stop_requested()) {
				return UciCommand::quit();
			}
			zAssert(!m_commands.empty());
			auto temp = m_commands.front();
			m_commands.pop_front();
			return temp;
		}

		void signalSearchStart() { //called by search thread
			std::scoped_lock l{ m_mutex };
			m_searchFinished = false;
		}
		void signalSearchEnd() {
			{
				std::scoped_lock l{ m_mutex };
				m_searchFinished = true;
			}
			m_cv.notify_one();
		}

		void cancel() {
			m_stopRequested.store(true);

			std::unique_lock l{ m_mutex };
			m_cv.wait(l, [this] {
				return m_searchFinished;
			});
			m_stopRequested.store(false);
		}
	};
}