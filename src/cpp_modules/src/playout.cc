#include "../include/playout.h"
#include "../include/eval.h"

const FastEvalWeights DEBUG_WEIGHTS = {
    /* avgHeightCoef= */ -1,
    /* burnCoef= */ -10,
    0,
    0,
    /* holeCoef= */ -40,
    /* tetrisCoef= */ 40,
    0,
    /* surfaceCoef= */ 1};
const EvalContext DEBUG_CONTEXT = {/* inputFrameTimeline= */ 1 << 4,
                                   /* scareHeight= */ 5,
                                   /* wellColumn= */ 9,
                                   /* countWellHoles= */ false};

SimState pickLockPlacement(GameState gameState,
                           EvalContext evalContext,
                           FastEvalWeights evalWeights,
                           OUT std::vector<SimState> &lockPlacements) {
  float bestSoFar = -99999999.0F;
  SimState bestPlacement = {};
  for (auto lockPlacement : lockPlacements) {
    float evalScore = fastEval(gameState, lockPlacement, evalContext, evalWeights);
    if (evalScore > bestSoFar) {
      bestSoFar = evalScore;
      bestPlacement = lockPlacement;
    }
  }
  maybePrint("\nBest placement: %d %d\n", bestPlacement.rotationIndex, bestPlacement.x - SPAWN_X);
  return bestPlacement;
}

float playSequence(GameState gameState, int pieceSequence[10]) {
  float totalReward = 0;
  for (int i = 0; i < 10; i++) {
    std::vector<SimState> lockPlacements;

    Piece piece = PIECE_LIST[pieceSequence[i]];
    moveSearch(gameState, piece, lockPlacements);
    SimState bestMove = pickLockPlacement(gameState, DEBUG_CONTEXT, DEBUG_WEIGHTS, lockPlacements);

    // On the last move, do a final evaluation
    if (i == 9){
      float evalScore = fastEval(gameState, bestMove, DEBUG_CONTEXT, DEBUG_WEIGHTS);
      if (PLAYOUT_LOGGING_ENABLED){
        gameState = advanceGameState(gameState, bestMove, DEBUG_CONTEXT);
        printBoard(gameState.board);
        printf("Best placement: %d %d\n\n", bestMove.rotationIndex, bestMove.x - SPAWN_X);
        printf("Cumulative reward: %01f\n", totalReward);
        printf("Final eval score: %01f\n", evalScore);
      }
      return totalReward + evalScore;
    }

    // Otherwise, update the state to keep playing
    int oldLines = gameState.lines;
    gameState = advanceGameState(gameState, bestMove, DEBUG_CONTEXT);
    totalReward += getLineClearFactor(gameState.lines - oldLines, DEBUG_WEIGHTS);
    if (PLAYOUT_LOGGING_ENABLED){
      printBoard(gameState.board);
      printf("Best placement: %d %d\n\n", bestMove.rotationIndex, bestMove.x - SPAWN_X);
    }
  }
  return -1; // Should never reach here
}