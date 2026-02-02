module;

#include <magic_enum/magic_enum.hpp>
#include <tracy/Tracy.hpp>

module Chess.MoveGeneration:LegalMoveGeneration;

import nlohmann.json;

import std;

import Chess.Assert;
import Chess.BitboardImage;
import Chess.PieceMap;
import Chess.RankCalculator;

import :ChainedMoveGenerator;
import :DestinationSquares;
import :KingMoveGeneration;
import :KnightMoveGeneration;
import :PawnMoveGeneration;
import :PieceAttackers;
import :PieceLocations;
import :Pin;
import :ReverseAttackGenerator;
import :SlidingMoveGenerators;

namespace chess {
	template<typename T>
	concept MoveAdder = std::invocable<T, MoveVector&, Move>;

	template<bool White, typename AllyPawnMoveGenerator, typename AllyPawnAttackGenerator,
			 typename EnemyPawnMoveGenerator, typename EnemyPawnAttackGenerator, Bitboard PromotionRank, 
			 bool DrawingBitboards>
	struct MoveGeneratorImpl {
		static inline AllyPawnMoveGenerator allyPawnMoveGenerator;
		static inline AllyPawnAttackGenerator allyPawnAttackGenerator;
		static inline EnemyPawnMoveGenerator enemyPawnMoveGenerator;
		static inline EnemyPawnAttackGenerator enemyPawnAttackGenerator;

		static_assert(PawnMoveGenerator<AllyPawnMoveGenerator>);
		static_assert(PawnMoveGenerator<EnemyPawnMoveGenerator>);

		static constexpr auto PAWN_ADDER = [](MoveVector& moves, Move move) {
			if (makeBitboard(move.to) & PromotionRank) {
				constexpr std::array PROMOTION_PIECES{ Queen, Rook, Bishop, Knight };
				for (auto piece : PROMOTION_PIECES) {
					move.promotionPiece = piece;
					moves.add(move);
				}
			} else {
				moves.add(move);
			}
		};

		static constexpr auto DEFAULT_MOVE_ADDER = [](MoveVector& moves, const Move& move) {
			moves.add(move);
		};

		template<MoveAdder MoveAdder>
		static void addMoves(PositionData& posData, Square piecePos, MoveGen destSquares, Piece pieceType,
			const PieceLocationData& pieceLocations, const PieceState& enemies, MoveAdder moveAdder)
		{
			posData.getAllySquares().destSquaresPinConsidered |= destSquares.all();
			posData.getAllySquares().allDestSquares        |= destSquares.all();

			Move move{ piecePos, Square::None, pieceType, Piece::None };

			while (nextSquare(destSquares.emptyDestSquares, move.to)) {
				moveAdder(posData.legalMoves, move);
			}
			auto capturedPieceSquares = destSquares.nonEmptyDestSquares & pieceLocations.enemies;
			while (nextSquare(capturedPieceSquares, move.to)) {
				move.capturedPiece = enemies.findPiece(move.to);
				moveAdder(posData.legalMoves, move);
			}
		}

		static void addCastlingMoves(const PositionData& posData, MoveGen& kingMoves, Bitboard allEnemySquares, const Position::ImmutableTurnData& turnData, 
			const PieceLocationData& pieceLocations)
		{
			auto isCastlingClear = [&](const auto& castleMove) {
				auto squaresClear = (castleMove.squaresBetweenRookAndKing & ~pieceLocations.empty) == 0;
				auto squaresNotChecked = (castleMove.mandatoryUncheckedSquares & allEnemySquares) == 0;
				return (!posData.isCheck && squaresClear && squaresNotChecked);
			};
			if (!posData.isCheck) {
				if (turnData.allies.castling.canCastleKingside() && isCastlingClear(turnData.allyKingside)) {
					kingMoves.emptyDestSquares |= makeBitboard(turnData.allyKingside.kingTo);
				}
				if (turnData.allies.castling.canCastleQueenside() && isCastlingClear(turnData.allyQueenside)) {
					kingMoves.emptyDestSquares |= makeBitboard(turnData.allyQueenside.kingTo);
				}
			} 
		}

		struct KingData {
			MoveGen kingSquares;
			AttackerData kingAttackerData;
		};

		template<bool IsWhite>
		static KingData calcFilteredKingMoves(bool isCheck, const PieceState& enemies, 
			const PieceLocationData& pieceLocations, Bitboard allEnemyDestSquares)
		{
			KingData ret;
			ret.kingSquares = kingMoveGenerator(pieceLocations.allyKing, pieceLocations.empty);
			ret.kingSquares &= ~allEnemyDestSquares;
			if (isCheck) {
				ret.kingAttackerData = calcAttackers(IsWhite, enemies, pieceLocations.empty, pieceLocations.allyKing);
				ret.kingSquares &= ~ret.kingAttackerData.indirectRays;
			}
			return ret;
		}

		template<PawnMoveGenerator PawnGenerator>
		static Bitboard calcNonPinDestSquares(const PieceState& pieces, const PieceLocationData& pieceLocations, PawnGenerator pawnMoveGenerator) {
			ZoneScoped;

			Bitboard ret = 0;
			ret |= queenMoveGenerator(pieces[Queen], pieceLocations.empty).all();
			ret |= bishopMoveGenerator(pieces[Bishop], pieceLocations.empty).all();
			ret |= rookMoveGenerator(pieces[Rook], pieceLocations.empty).all();
			ret |= knightMoveGenerator(pieces[Knight], pieceLocations.empty).all();
			ret |= pawnMoveGenerator(pieces[Pawn], pieceLocations.empty, ALL_SQUARES).all(); //ensure pawns actually defend fellow enemy pieces
			return ret;
		}

		static void addEnPassantMoves(PositionData& posData, const PinMap& pinMap, const AttackerData& kingAttackers, 
			const PieceState& enemies, Bitboard pawns, Square jumpedEnemyPawn, const PieceLocationData& pieceLocations)
		{
			auto enPassantData = allyPawnAttackGenerator(pawns, jumpedEnemyPawn);
			if (!enPassantData.pawns) {
				return;
			}
			auto from = Square::None;
			while (nextSquare(enPassantData.pawns, from)) {
				if (pinMap[from].mustMoveInPinRay) { //impossible for an en passant pawn to be pinned by a pawn, so it can't take the double jumped pawn
					continue;
				}
				PieceLocationData newPieceLocations{
					pieceLocations.allyKing,
					pieceLocations.allies & ~makeBitboard(from),
					pieceLocations.enemies & ~makeBitboard(jumpedEnemyPawn)
				};
				auto newSlidingAttackers = calcSlidingAttackers(enemies, newPieceLocations.empty, newPieceLocations.allyKing);
				if ((newSlidingAttackers.attackers.calcAllLocations() & ~kingAttackers.attackers.calcAllLocations()) != 0) { //ally pawn is actually pinned from behind the enemy pawn
					continue;
				}
				posData.legalMoves.add(from, enPassantData.squareInFrontOfEnemyPawn, jumpedEnemyPawn, Pawn, Pawn, Piece::None);
				MoveGen gen{ 0, makeBitboard(enPassantData.squareInFrontOfEnemyPawn) };

				posData.getAllySquares().allDestSquares |= gen.all();
				posData.getAllySquares().destSquaresPinConsidered |= gen.all();
			}
		}

		static PositionData calcAllLegalMoves(const Position::ImmutableTurnData& turnData) {
			PositionData ret{ White };
			auto& allies  = turnData.allies;
			auto& enemies = turnData.enemies;
			auto [allySquares, enemySquares] = White ? std::tie(ret.whiteSquares, ret.blackSquares) : std::tie(ret.blackSquares, ret.whiteSquares);

			PieceLocationData pieceLocations{ allies[King], allies.calcAllLocations(), enemies.calcAllLocations() };
			allySquares.allDestSquares = calcNonPinDestSquares(allies, pieceLocations, allyPawnMoveGenerator);

			PieceLocationData pieceLocationsEnemyPOV{ enemies[King], enemies.calcAllLocations(), allies.calcAllLocations() };
			enemySquares.allDestSquares = calcNonPinDestSquares(enemies, pieceLocationsEnemyPOV, enemyPawnMoveGenerator);

			ret.isCheck = pieceLocations.allyKing & enemySquares.allDestSquares;
			auto [allyKingSquares, allyKingAttackerData]   = calcFilteredKingMoves<White>(ret.isCheck, enemies, pieceLocations, enemySquares.allDestSquares);
			auto [enemyKingSquares, enemyKingAttackerData] = calcFilteredKingMoves<!White>(false, allies, pieceLocationsEnemyPOV, allySquares.allDestSquares);

			//kings can't move to the same squares
			auto sharedKingSquares = allyKingSquares.all() & enemyKingSquares.all();
			allyKingSquares &= ~sharedKingSquares;
			enemyKingSquares &= ~sharedKingSquares;

			addCastlingMoves(ret, allyKingSquares, enemySquares.allDestSquares, turnData, pieceLocations);
			addMoves(ret, nextSquare(pieceLocations.allyKing), allyKingSquares, King, pieceLocations, turnData.enemies, DEFAULT_MOVE_ADDER);

			allySquares.destSquaresPinConsidered |= allyKingSquares.all();
			enemySquares.destSquaresPinConsidered |= enemyKingSquares.all();

			if (allyKingAttackerData.hasMultipleAttackers()) { //if there are multiple checks, we have to move the king
				return ret;
			}

			auto attackedAllies = enemySquares.allDestSquares & pieceLocations.allies;
			auto allyPinMap = calcPinnedAllies(allies, enemies, attackedAllies, allyKingAttackerData, pieceLocations);

			auto attackedEnemies = allySquares.allDestSquares & pieceLocations.enemies;
			auto enemyPinMap = calcPinnedAllies(enemies, allies, attackedEnemies, enemyKingAttackerData, pieceLocations);

			//add ally moves
			forEachDestSquare<White>(allies, allyPinMap, allyKingAttackerData, pieceLocations, [&](Square square, Piece pieceType, const MoveGen& destSquares) {
				if (pieceType == Pawn) {
					addMoves(ret, square, destSquares, pieceType, pieceLocations, enemies, PAWN_ADDER);
				} else {
					addMoves(ret, square, destSquares, pieceType, pieceLocations, enemies, DEFAULT_MOVE_ADDER);
				}
				allySquares.destSquaresPinConsidered |= destSquares.all();
			});
			if (enemies.doubleJumpedPawn != Square::None) { //todo: add en passant moves to allySquares 
				addEnPassantMoves(ret, allyPinMap, allyKingAttackerData, enemies, allies[Pawn], enemies.doubleJumpedPawn, pieceLocations);
			}

			//add enemy moves 
			forEachDestSquare<!White>(enemies, enemyPinMap, enemyKingAttackerData, pieceLocationsEnemyPOV, [&](Square, Piece, const MoveGen& destSquares) {
				enemySquares.destSquaresPinConsidered |= destSquares.all();
			});

			return ret;
		}
	};

	template<bool DrawingBitboards>
	PositionData calcAllLegalMovesImpl(const Position& pos) {
		ZoneScoped;

		auto turnData = pos.getTurnData();

		if (turnData.isWhite) {
			using MoveGenerator = MoveGeneratorImpl<true, WhitePawnMoveGenerator, WhitePawnAttackGenerator,
				BlackPawnMoveGenerator, BlackPawnAttackGenerator, calcRank<8>(), DrawingBitboards>;
			return MoveGenerator::calcAllLegalMoves(turnData);
		} else {
			using MoveGenerator = MoveGeneratorImpl<false, BlackPawnMoveGenerator, BlackPawnAttackGenerator,
				WhitePawnMoveGenerator, WhitePawnAttackGenerator, calcRank<1>(), DrawingBitboards>;
			return MoveGenerator::calcAllLegalMoves(turnData);
		}
	}
	
	PositionData calcPositionData(const Position& pos) {
		return calcAllLegalMovesImpl<false>(pos);
	}
	PositionData calcPositionDataAndDrawBitboards(const Position& pos) {
		return calcAllLegalMovesImpl<true>(pos);
	}
}