module;

#include <magic_enum/magic_enum.hpp>
#include <tracy/Tracy.hpp>

module Chess.MoveGeneration:PieceAttackers;

import Chess.Square;

import :KingMoveGeneration;
import :KnightMoveGeneration;
import :RayTable;
import :SlidingMoveGenerators;

namespace chess {
	struct RayData {
		Bitboard directRay = 0;
		Bitboard xRay = 0;
	};

	template<typename EnemyMoveGenerator>
	RayData calcRayData(Square currAttacker, Square attackedPiece, Bitboard normalEmpty, Bitboard boardWithoutAttackedPiece, 
		EnemyMoveGenerator enemyMoveGenerator)
	{
		auto attackerBoard = makeBitboard(currAttacker);
		auto directAttackRay = getRay(attackedPiece, currAttacker);

		auto directlyAttackedSquares = enemyMoveGenerator(attackerBoard, normalEmpty).all();
		auto indirectlyAttackedSquares = enemyMoveGenerator(attackerBoard, boardWithoutAttackedPiece).all() & ~directlyAttackedSquares;
		
		return { directAttackRay, indirectlyAttackedSquares };
	}

	template<SlidingMoveGenerator EnemyMoveGenerator>
	void calcSlidingAttackersImpl(AttackerData& attackerData, Bitboard attackedPiece, Piece attackerType, Bitboard possibleAttackers, 
		Bitboard empty, EnemyMoveGenerator enemyMoveGenerator)
	{
		auto reverseAttacks = enemyMoveGenerator(attackedPiece, empty);

		auto currAttacker = Square::None;
		auto attackers = reverseAttacks.nonEmptyDestSquares & possibleAttackers;
		attackerData.attackers[attackerType] |= attackers;

		auto attackedPieceSquare = nextSquare(attackedPiece);
		auto boardWithoutAttackedPiece = empty | attackedPiece; //ensure indirectly attacked rays are calculated as well
		while (nextSquare(attackers, currAttacker)) {
			auto [directRay, xRay] = calcRayData(currAttacker, attackedPieceSquare, empty, boardWithoutAttackedPiece, enemyMoveGenerator);
			attackerData.rays |= directRay;
			attackerData.indirectRays |= xRay;
		}
	}

	void calcSlidingAttackers(AttackerData& attackerData, const PieceState& enemies, Bitboard empty, Bitboard attackedPiece) {
		calcSlidingAttackersImpl(attackerData, attackedPiece, Bishop, enemies[Bishop], empty, bishopMoveGenerator);
		calcSlidingAttackersImpl(attackerData, attackedPiece, Rook, enemies[Rook], empty, rookMoveGenerator);
		calcSlidingAttackersImpl(attackerData, attackedPiece, Queen, enemies[Queen], empty, queenMoveGenerator);
	}

	AttackerData calcSlidingAttackers(const PieceState& enemies, Bitboard empty, Bitboard attackedPiece) {
		AttackerData ret;
		calcSlidingAttackers(ret, enemies, empty, attackedPiece);
		return ret;
	}

	void calcKnightAttackers(AttackerData& attackerData, Bitboard attackedPiece, Bitboard enemyKnights) {
		auto reverseAttacks = knightMoveGenerator(attackedPiece, ~enemyKnights);
		attackerData.attackers[Knight] |= reverseAttacks.nonEmptyDestSquares;
	}

	template<PawnMoveGenerator ReversePawnAttackGenerator>
	void calcPawnAttackers(AttackerData& attackerData, Bitboard attackedPiece, Bitboard enemyPawns, 
		ReversePawnAttackGenerator reversePawnAttackGenerator)
	{
		auto reverseAttacks = reversePawnAttackGenerator(attackedPiece, enemyPawns);
		auto pawnAttackers = reverseAttacks.nonEmptyDestSquares;
		attackerData.attackers[Pawn] |= pawnAttackers;
	}

	void printSquares(const std::string& s, Bitboard b) {
		std::println("{} contains: ", s);
		auto currSquare = Square::None;
		while (nextSquare(b, currSquare)) {
			std::println("{}", magic_enum::enum_name(currSquare));
		}
	}

	AttackerData calcAttackers(bool isWhite, const PieceState& enemies, Bitboard empty, Bitboard attackedPiece) {
		ZoneScoped;

		AttackerData ret;

		//see if the enemy king is attacking the piece (impossible if attacked piece is a king or sliding piece)
		auto enemyKingAttacks = kingMoveGenerator(enemies[King], empty);
		if (enemyKingAttacks.nonEmptyDestSquares & attackedPiece) {
			ret.attackers[King] |= enemies[King];
		}

		//calculate the rooks, bishops, and queens attacking the piece
		calcSlidingAttackers(ret, enemies, empty, attackedPiece);

		//calculate knight attackers
		calcKnightAttackers(ret, attackedPiece, enemies[Knight]);

		//calculate pawn attackers
		if (isWhite) {
			calcPawnAttackers(ret, attackedPiece, enemies[Pawn], whitePawnAttackGenerator);
		} else {
			calcPawnAttackers(ret, attackedPiece, enemies[Pawn], blackPawnAttackGenerator);
		}

		return ret;
	}
}