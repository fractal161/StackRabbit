#include "playout.hpp"
#include "eval.hpp"
#include "utils.hpp"
#include "params.hpp"
#include "../data/canonical_sequences.hpp"

using namespace std;

/** Selects the highest value lock placement using the fast eval function. */
SimState pickLockPlacement(GameState gameState,
                           const EvalContext *evalContext,
                           OUT vector<SimState> &lockPlacements) {
  float bestSoFar = evalContext->weights.deathCoef;
  SimState bestPlacement = {};
  for (auto lockPlacement : lockPlacements) {
    GameState newState = advanceGameState(gameState, lockPlacement, evalContext);
    float evalScore = fastEval(gameState, newState, lockPlacement, evalContext);
    if (evalScore > bestSoFar) {
      bestSoFar = evalScore;
      bestPlacement = lockPlacement;
    }
  }
  maybePrint("\nBest placement: %d %d\n", bestPlacement.rotationIndex, bestPlacement.x - SPAWN_X);
  return bestPlacement;
}


/**
 * Plays out a starting state 10 moves into the future.
 * @returns the total value of the playout (intermediate rewards + eval of the final board)
 */
float playSequence(GameState gameState, const PieceRangeContext pieceRangeContextLookup[3], const int pieceSequence[SEQUENCE_LENGTH], int playoutLength) {
  float totalReward = 0;
  for (int i = 0; i < playoutLength; i++) {
    // Figure out modes and eval context
    const EvalContext evalContextRaw = getEvalContext(gameState, pieceRangeContextLookup);
    const EvalContext *evalContext = &evalContextRaw;
    FastEvalWeights weights = getWeights(evalContext->aiMode);

    // Get the lock placements
    std::vector<SimState> lockPlacements;
    Piece piece = PIECE_LIST[pieceSequence[i]];
    moveSearch(gameState, piece, evalContext->pieceRangeContext.inputFrameTimeline, lockPlacements);

    if (lockPlacements.size() == 0) {
      return weights.deathCoef;
    }

    // Pick the best placement
    SimState bestMove = pickLockPlacement(gameState, evalContext, lockPlacements);

    // On the last move, do a final evaluation
    if (i == playoutLength - 1) {
      GameState nextState = advanceGameState(gameState, bestMove, evalContext);
      float evalScore = fastEval(gameState, nextState, bestMove, evalContext);
      if (PLAYOUT_LOGGING_ENABLED) {
        gameState = nextState;
        printBoard(gameState.board);
        printf("Best placement: %c %d, %d\n\n", bestMove.piece.id, bestMove.rotationIndex, bestMove.x - SPAWN_X);
        printf("Cumulative reward: %01f\n", totalReward);
        printf("Final eval score: %01f\n", evalScore);
      }
      return totalReward + evalScore;
    }

    // Otherwise, update the state to keep playing
    int oldLines = gameState.lines;
    gameState = advanceGameState(gameState, bestMove, evalContext);
    FastEvalWeights rewardWeights = evalContext->aiMode == DIG ? getWeights(STANDARD) : weights; // When the AI is digging, still deduct from the overall value of the sequence at standard levels
    totalReward += getLineClearFactor(gameState.lines - oldLines, rewardWeights, evalContext->shouldRewardLineClears);
    if (PLAYOUT_LOGGING_ENABLED) {
      printBoard(gameState.board);
      printf("Best placement: %c %d, %d\n\n", bestMove.piece.id, bestMove.rotationIndex, bestMove.x - SPAWN_X);
    }
  }
  return -1; // Doesn't reach here, always returns from i == 9 case
}


float getPlayoutScore(GameState gameState, const PieceRangeContext pieceRangeContextLookup[3], int offsetIndex){
  if (LOGGING_ENABLED) {
    return 0;
  }

  int totalPlayouts = NUM_PLAYOUTS_LONG + NUM_PLAYOUTS_SHORT;
  int offset = offsetIndex * (totalPlayouts / 7 + 1); // Index into the sequences in batches, with batch size equal to the total number of playouts

  float longPlayoutScore = 0;
  for (int i = 0; i < NUM_PLAYOUTS_LONG; i++) {
    // Do one playout
    const int *pieceSequence = canonicalPieceSequences + (offset + i) * SEQUENCE_LENGTH; // Index into the mega array of piece sequences;
    float playoutScore = playSequence(gameState, pieceRangeContextLookup, pieceSequence, PLAYOUT_LENGTH_LONG);
    longPlayoutScore += playoutScore;
  }
  // printf("(A) longPlayoutScore %f \n", longPlayoutScore);
  
  float shortPlayoutScore = 0;
  for (int i = 0; i < NUM_PLAYOUTS_SHORT; i++) {
    // Do one playout
    const int *pieceSequence = canonicalPieceSequences + (offset + i) * SEQUENCE_LENGTH; // Index into the mega array of piece sequences;
    float playoutScore = playSequence(gameState, pieceRangeContextLookup, pieceSequence, PLAYOUT_LENGTH_SHORT);
    shortPlayoutScore += playoutScore;
  }
  // printf("    shortPlayoutScore %f \n", shortPlayoutScore);

  
  return (NUM_PLAYOUTS_SHORT == 0 ? 0 : (shortPlayoutScore / NUM_PLAYOUTS_SHORT)) +
         (NUM_PLAYOUTS_LONG == 0 ? 0 : (longPlayoutScore / NUM_PLAYOUTS_LONG));
}