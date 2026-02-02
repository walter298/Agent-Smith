module;

#include <tracy/Tracy.hpp>

module Chess.MoveSearch;

import std;
import BS.thread_pool;

import Chess.Arena;
import Chess.Assert;
import Chess.DebugPrint;
import Chess.EasyRandom;
import Chess.Evaluation;
import Chess.MoveGeneration;
import Chess.Position.RepetitionMap;
import Chess.Rating;

import :MoveOrdering;
import :MoveHasher;
import :Node;
import :TranspositionTable;

namespace chess {
	class AlphaBeta {
	private:
		Rating m_alpha = worstPossibleRating<true>();
		Rating m_beta = worstPossibleRating<false>();
	public:
		void updateAlpha(Rating childRating) {
			m_alpha = std::max(childRating, m_alpha);
		}
		void updateBeta(Rating childRating) {
			m_beta = std::min(childRating, m_beta);
		}

		template<bool Maximizing>
		void update(Rating childRating) {
			if constexpr (Maximizing) {
				updateAlpha(childRating);
			} else {
				updateBeta(childRating);
			}
		}

		bool canPrune() const {
			return m_beta <= m_alpha;
		}

		Rating getAlpha() const {
			return m_alpha;
		}
		Rating getBeta() const {
			return m_beta;
		}
	};

	struct MoveRating {
		Move move = Move::null();
		Rating rating = 0_rt;
		bool invalidTTEntry = false;
		std::optional<SafeInt<std::uint8_t>> checkmateLevel = std::nullopt;
	};

	class Searcher {
	private:
		static constexpr SafeInt<std::uint8_t> RANDOMIZATION_CUTOFF{ 3 }; //todo: make this a fraction of the max depth 
		std::mt19937 m_urbg;
		bool m_helper = false;
		const std::atomic_bool* m_stopRequested;

		static constexpr auto MAX_DEPTH = 30uz;
		static constexpr auto MAX_KILLER_MOVES = 3uz;
		struct KillerMoveEntries {
			std::array<Move, MAX_KILLER_MOVES> killerMoves{};
			size_t index = 0;
		};
		std::array<KillerMoveEntries, MAX_DEPTH> m_killerMoves{};
	public:
		SafeInt<std::uint8_t> depth = 0_su8;

		Searcher(bool helper, const std::atomic_bool* stopRequested)
			: m_urbg{ std::random_device{}() }, m_helper{ helper }, m_stopRequested{ stopRequested }
		{
			for (auto& killerMoves : m_killerMoves) {
				std::ranges::fill(killerMoves.killerMoves, Move::null());
				killerMoves.index = 0;
			}
		}

		Rating getVotingWeight(const MoveRating& moveRating, Rating& worstScore, Rating maxScoreDiff) const {
			zAssert(maxScoreDiff >= 0_rt);

			auto ret = 1_rt;
			ret += Rating{ static_cast<Rating::Int>(std::pow(2.0, static_cast<double>(depth.get()))) };
			
			//give up to 20% boost depending on how good the score is
			if (maxScoreDiff != 0_rt) {
				ret *= (1.2_rt * (moveRating.rating - worstScore) / maxScoreDiff);
			}

			if (moveRating.checkmateLevel) {
				ret += ret / std::max(static_cast<Rating>(moveRating.checkmateLevel->get()), 1_rt);
			}

			return ret;
		}

		bool isHelper() const {
			return m_helper;
		}
	private:
		static bool wouldMakeRepetition(const Position& pos, Move pvMove, const RepetitionMap& repetitionMap) {
			Position child{ pos, pvMove };
			auto repetitionCount = repetitionMap.getPositionCount(child) + 1; //add 1 since we haven't actually pushed this position yet
			return repetitionCount >= 2; //return 2 (not 3) because the opposing player could then make a threefold repetition after this
		}

		template<bool Maximizing>
		MoveRating tryShortCircuit(const Node& node, AlphaBeta alphaBeta) {
			ZoneScoped;

			if (node.getPositionData().legalMoves.empty()) {
				MoveRating ret;

				if (node.getPositionData().isCheckmate()) {
					ret.rating = checkmatedRating<Maximizing>();
					ret.checkmateLevel = node.getLevel();
				}
				return ret;
			}

			if (node.getRepetitionMap().getPositionCount(node.getPos()) >= 3) {
				return { Move::null(), 0_rt, true };
			}

			auto pvMove = Move::null();

			if (m_stopRequested->load()) {
				return { Move::null(), node.getRating(), true };
			}

			bool canUseEntry = !(m_helper && node.getLevel() == 0_su8);

			if (!m_stopRequested->load() && canUseEntry) {
				if (auto entryRes = getPositionEntry(node.getPos(), node.getRemainingDepth())) {
					const auto& entry = *entryRes;
					pvMove = entry.bestMove;

					auto& legalMoves = node.getPositionData().legalMoves;
					bool validMove = std::ranges::contains(legalMoves, pvMove) && //todo: make this fast
						!wouldMakeRepetition(node.getPos(), entry.bestMove, node.getRepetitionMap()) &&
						entry.depth >= node.getRemainingDepth();

					if (validMove) {
						switch (entry.bound) {
						case InWindow:
							return { entry.bestMove, entry.rating, false };
							break;
						case LowerBound:
							if (entry.rating >= alphaBeta.getBeta()) {
								return { entry.bestMove, entry.rating, false };
							} else {
								alphaBeta.updateAlpha(entry.rating);
							}
							break;
						case UpperBound:
							if (entry.rating <= alphaBeta.getAlpha()) {
								return { entry.bestMove, entry.rating, false };
							} else {
								alphaBeta.updateBeta(entry.rating);
							}
							break;
						}
					}
				}
			}
			if (node.isDone()) {
				return { Move::null(), node.getRating(), false }; //safe to return Move::null(), as node is never done at the root
			}
			return bestChildPosition<Maximizing>(node, pvMove, alphaBeta);
		}

		template<bool Maximizing>
		MoveRating bestChildPosition(const Node& node, const Move& pvMove, AlphaBeta alphaBeta) {
			ZoneScoped;

			auto originalAlphaBeta = alphaBeta;

			auto& killerMoves = m_killerMoves[node.getLevel().get()];
			auto movePriorities = getMovePriorities(node, pvMove, std::span{ killerMoves.killerMoves.data(), MAX_KILLER_MOVES });;
			if (m_helper && node.getLevel() < RANDOMIZATION_CUTOFF) {
				std::ranges::shuffle(movePriorities, m_urbg);
			}

			MoveRating bestRating{ Move::null(), worstPossibleRating<Maximizing>(), false };
			
			auto bound = InWindow;
			bool didNotPrune = true;

			for (const auto& movePriority : movePriorities) {
				Node child{ node, movePriority };
				auto childRating = tryShortCircuit<!Maximizing>(child, alphaBeta);

				//ensure that we don't accidentally pick a move whose depth priority was trimmed by LMR
				if (movePriority.isTrimmed()) {
					auto mayChooseThisMove = Maximizing ? childRating.rating >= alphaBeta.getAlpha() :
														  childRating.rating <= alphaBeta.getBeta();
					if (mayChooseThisMove) {
						MovePriority fullMovePriority{ movePriority.getMove(), node.getRemainingDepth() - 1_su8 };
						Node newChild{ node, fullMovePriority };
						childRating = tryShortCircuit<!Maximizing>(newChild, alphaBeta);
					}
				}

				if constexpr (Maximizing) {
					if (childRating.rating > bestRating.rating) {
						bestRating = childRating;
						bestRating.move = movePriority.getMove();
					}
				} else {
					if (childRating.rating < bestRating.rating) {
						bestRating = childRating;
						bestRating.move = movePriority.getMove();
					}
				}

				alphaBeta.update<Maximizing>(bestRating.rating);
				if (alphaBeta.canPrune()) {
					//add killer move
					if (movePriority.getMove().capturedPiece == Piece::None) {
						killerMoves.killerMoves[killerMoves.index] = movePriority.getMove();
						killerMoves.index = killerMoves.index + 1 == MAX_KILLER_MOVES ? 0 : killerMoves.index + 1;
					}
					
					bound = Maximizing ? LowerBound : UpperBound;
					didNotPrune = false;
					break;
				}

				if (childRating.rating == checkmatedRating<!Maximizing>()) {
					break;
				}
			}

			if (didNotPrune) {
				if constexpr (Maximizing) {
					if (bestRating.rating <= originalAlphaBeta.getAlpha()) {
						bound = UpperBound;
					}
				} else {
					if (bestRating.rating >= originalAlphaBeta.getBeta()) {
						bound = LowerBound;
					}
				}
			}

			if (!bestRating.invalidTTEntry) {
				TTEntry newEntry{ bestRating.move, node.getRemainingDepth(), bound, bestRating.rating };
				storePositionEntry(node.getPos(), newEntry);
			}

			bestRating.invalidTTEntry = false; //don't propagate repetition flag up the tree (stop requests will be rechecked)
			return bestRating;
		}

		template<bool Maximizing>
		MoveRating startAlphaBetaSearch(const Position& pos, SafeInt<std::uint8_t> depth, RepetitionMap repetitionMap) {
			AlphaBeta alphaBeta;
			Node root{ pos, depth, repetitionMap };
			return tryShortCircuit<Maximizing>(root, alphaBeta);
		}

		template<bool Maximizing>
		MoveRating iterativeDeepening(const Position& pos, const RepetitionMap& repetitionMap) {
			for (auto iterDepth = 1_su8; iterDepth < depth; ++iterDepth) {
				startAlphaBetaSearch<Maximizing>(pos, iterDepth, repetitionMap);
			}
			return startAlphaBetaSearch<Maximizing>(pos, depth, repetitionMap);
		}
	public:
		MoveRating operator()(const Position& pos, const RepetitionMap& repetitionMap) {
			if (pos.isWhite()) {
				return iterativeDeepening<true>(pos, repetitionMap);
			} else {
				return iterativeDeepening<false>(pos, repetitionMap);
			}
		}
	};

	const auto THREAD_COUNT = std::thread::hardware_concurrency();
	constexpr auto MAIN_THREAD_INDEX = 0uz;

	struct AsyncSearchState {
		BS::thread_pool<> pool{ THREAD_COUNT };
		std::atomic_bool stopRequested = false;
		std::vector<Searcher> searchers;

		AsyncSearchState() {
			searchers.reserve(THREAD_COUNT);
			searchers.emplace_back(false, &stopRequested); //insert main thread
			if (THREAD_COUNT > 1) {
				for (auto i = 0uz; i < THREAD_COUNT - 1; i++) { //insert helper threads
					searchers.emplace_back(true, &stopRequested);
				}
			}
		}

		void assignDepths(SafeInt<std::uint8_t> maxDepth) {
			zAssert(maxDepth >= 1_su8);
			
			for (auto&& [i, searcher] : std::views::enumerate(searchers)) {
				if (!searcher.isHelper()) {
					searcher.depth = maxDepth;
				} else {
					auto d = (SafeInt{ static_cast<std::uint8_t>(i) } % 2_su8) == 0_su8 ? 1_su8 : 0_su8;
					auto depth = maxDepth == 1_su8 ? maxDepth : maxDepth - d;
					searcher.depth = depth;
				}
			}
		}
	};

	AsyncSearch::AsyncSearch()
		: m_state{ std::make_shared<AsyncSearchState>() }
	{
	}

	SearchResult voteForBestMove(const std::vector<Searcher>& searchers, const std::vector<MoveRating>& moves) {
		auto anyPathsLeadToCheckmate = std::ranges::any_of(moves, [](const MoveRating& m) {
			return m.checkmateLevel.has_value();
		});
		if (anyPathsLeadToCheckmate) {
			auto quickestCheckmate = std::ranges::min_element(moves, std::less{}, [](const MoveRating& mr) {
				if (!mr.checkmateLevel) {
					return 255_su8;
				}
				return *mr.checkmateLevel;
			});
			return { quickestCheckmate->move, quickestCheckmate->rating };
		}

		auto [worstIt, bestIt] = std::ranges::minmax_element(moves, std::less{}, [](const MoveRating& mr) {
			return mr.rating;
		});
		auto worstScore = worstIt->rating;
		auto maxScoreDiff = bestIt->rating - worstScore;

		std::unordered_map<Move, Rating, MoveHasher> moveRatings;
		auto bestMove = Move::null();
		auto bestVoteRating = 0_rt;

		for (const auto& [moveRating, searcher] : std::views::zip(moves, searchers)) {
			auto& voteRating = moveRatings[moveRating.move];
			std::println("Searcher thinks {} is {}", moveRating.move.getUCIString(), moveRating.rating.get());
			voteRating += searcher.getVotingWeight(moveRating, worstScore, maxScoreDiff);
			if (voteRating > bestVoteRating) {
				bestVoteRating = voteRating;
				bestMove = moveRating.move;
			}
		}

		return { bestMove, bestVoteRating };
	} 

	SearchResult findBestMoveImpl(std::shared_ptr<AsyncSearchState> state, Position pos, SafeInt<std::uint8_t> depth, RepetitionMap repetitionMap) {
		state->assignDepths(depth);
		state->stopRequested.store(false);

		auto moveCandidateFutures = state->pool.submit_sequence(0uz, state->searchers.size(), [&](size_t i) {
			return state->searchers[i](pos, repetitionMap);
		});
		
		auto moveCandidates = moveCandidateFutures.get();
		zAssert(!moveCandidates.empty());

		if (state->stopRequested.load()) {
			return SearchResult{};
		}

		//move candidates could contain null moves if a stop was requested, or if there is checkmate
		auto hasNullMove = std::ranges::any_of(moveCandidates, [](const MoveRating& mr) {
			return mr.move == Move::null();
		});
		if (hasNullMove) {
			return SearchResult{};
		}

		return voteForBestMove(state->searchers, moveCandidates);
	}

	SearchResult AsyncSearch::findBestMove(const Position& pos, SafeInt<std::uint8_t> depth, const RepetitionMap& repetitionMap) {
		ZoneScoped;

		updateTTAge();

		return findBestMoveImpl(m_state, pos, depth, repetitionMap);
	}

	void AsyncSearch::cancel() {
		m_state->stopRequested.store(true);
	}
}