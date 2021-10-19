package contention.benchmark;

import java.util.Formatter;
import java.util.HashMap;
import java.util.Locale;
import java.util.Map;

/**
 * This class provides statistics collecting functionality for STMs 
 * Adapted from Yoav Cohen's code as used in Deuce.
 * 
 * @author Vincent Gramoli
 * 
 */
public class Statistics {

	public enum AbortType {
		ALL, BETWEEN_SUCCESSIVE_READS, BETWEEN_READ_AND_WRITE, EXTEND_ON_READ, WRITE_AFTER_READ, LOCKED_ON_WRITE, LOCKED_BEFORE_READ, LOCKED_BEFORE_ELASTIC_READ, LOCKED_ON_READ, INVALID_COMMIT, INVALID_SNAPSHOT
	}

	public enum CommitType {
		ALL, READONLY, ELASTIC, UPDATE
	}

	public enum ValidationType {
		ALL, READ_BACK, READ_SET, COMMIT_BACK, COMMIT_RS
	}

	private static final Map<Integer, Statistics> statsMap = new HashMap<Integer, Statistics>();
	private static int[] txAttemptsHistBins;

	static {
		String histStr = System.getProperty("txDurationHist");
		if (histStr == null) {
			histStr = "1,2,5,20";
		}
		String[] histStrArr = histStr.split(",");
		txAttemptsHistBins = new int[histStrArr.length + 1];
		for (int i = 0; i < txAttemptsHistBins.length - 1; i++) {
			String limitStr = histStrArr[i];
			int limit = Integer.valueOf(limitStr);
			txAttemptsHistBins[i] = limit;
		}
		txAttemptsHistBins[txAttemptsHistBins.length - 1] = 100;
	}

	public static int getTotalStarts() {
		int totalStarts = 0;
		for (Map.Entry<Integer, Statistics> entry : statsMap.entrySet()) {
			Statistics statistics = entry.getValue();
			totalStarts += statistics.starts;
		}
		return totalStarts;
	}

	public static int getTotalCommits() {
		int totalCommits = 0;
		for (Map.Entry<Integer, Statistics> entry : statsMap.entrySet()) {
			Statistics statistics = entry.getValue();
			totalCommits += statistics.commits;
		}
		return totalCommits;
	}

	public static void resetAll() {
		statsMap.clear();
	}

	public static int getNumCommitsElastic() {
		int total = 0;
		for (Map.Entry<Integer, Statistics> entry : statsMap.entrySet()) {
			Statistics statistics = entry.getValue();
			total += statistics.getCommits(CommitType.ELASTIC);
		}
		return total;
	}

	public static int getNumCommitsUpdate() {
		int total = 0;
		for (Map.Entry<Integer, Statistics> entry : statsMap.entrySet()) {
			Statistics statistics = entry.getValue();
			total += statistics.getCommits(CommitType.UPDATE);
		}
		return total;
	}

	public static int getNumCommitsReadOnly() {
		int total = 0;
		for (Map.Entry<Integer, Statistics> entry : statsMap.entrySet()) {
			Statistics statistics = entry.getValue();
			total += statistics.getCommits(CommitType.READONLY);
		}
		return total;
	}

	public static int getTotalValidations() {
		int totalAborts = 0;
		for (Map.Entry<Integer, Statistics> entry : statsMap.entrySet()) {
			Statistics statistics = entry.getValue();
			totalAborts += statistics.getValidation(ValidationType.ALL);
		}
		return totalAborts;
	}

	public static int getReadBackValidations() {
		int totalAborts = 0;
		for (Map.Entry<Integer, Statistics> entry : statsMap.entrySet()) {
			Statistics statistics = entry.getValue();
			totalAborts += statistics.getValidation(ValidationType.READ_BACK);
		}
		return totalAborts;
	}

	public static int getReadSetValidations() {
		int totalAborts = 0;
		for (Map.Entry<Integer, Statistics> entry : statsMap.entrySet()) {
			Statistics statistics = entry.getValue();
			totalAborts += statistics.getValidation(ValidationType.READ_SET);
		}
		return totalAborts;
	}

	public static int getCommitBackValidations() {
		int totalAborts = 0;
		for (Map.Entry<Integer, Statistics> entry : statsMap.entrySet()) {
			Statistics statistics = entry.getValue();
			totalAborts += statistics.getValidation(ValidationType.COMMIT_BACK);
		}
		return totalAborts;
	}

	public static int getCommitRSValidations() {
		int totalAborts = 0;
		for (Map.Entry<Integer, Statistics> entry : statsMap.entrySet()) {
			Statistics statistics = entry.getValue();
			totalAborts += statistics.getValidation(ValidationType.COMMIT_RS);
		}
		return totalAborts;
	}

	public static int getTotalAborts() {
		int totalAborts = 0;
		for (Map.Entry<Integer, Statistics> entry : statsMap.entrySet()) {
			Statistics statistics = entry.getValue();
			totalAborts += statistics.getAborts(AbortType.ALL);
		}
		return totalAborts;
	}

	public static int getNumAbortsBetweenSuccessiveReads() {
		int total = 0;
		for (Map.Entry<Integer, Statistics> entry : statsMap.entrySet()) {
			Statistics statistics = entry.getValue();
			total += statistics.getAborts(AbortType.BETWEEN_SUCCESSIVE_READS);
		}
		return total;
	}

	public static int getNumAbortsBetweenReadAndWrite() {
		int total = 0;
		for (Map.Entry<Integer, Statistics> entry : statsMap.entrySet()) {
			Statistics statistics = entry.getValue();
			total += statistics.getAborts(AbortType.BETWEEN_READ_AND_WRITE);
		}
		return total;
	}

	public static int getNumAbortsExtendOnRead() {
		int total = 0;
		for (Map.Entry<Integer, Statistics> entry : statsMap.entrySet()) {
			Statistics statistics = entry.getValue();
			total += statistics.getAborts(AbortType.EXTEND_ON_READ);
		}
		return total;
	}

	public static int getNumAbortsWriteAfterRead() {
		int total = 0;
		for (Map.Entry<Integer, Statistics> entry : statsMap.entrySet()) {
			Statistics statistics = entry.getValue();
			total += statistics.getAborts(AbortType.WRITE_AFTER_READ);
		}
		return total;
	}

	public static int getNumAbortsLockedOnWrite() {
		int total = 0;
		for (Map.Entry<Integer, Statistics> entry : statsMap.entrySet()) {
			Statistics statistics = entry.getValue();
			total += statistics.getAborts(AbortType.LOCKED_ON_WRITE);
		}
		return total;
	}

	public static int getNumAbortsLockedOnRead() {
		int total = 0;
		for (Map.Entry<Integer, Statistics> entry : statsMap.entrySet()) {
			Statistics statistics = entry.getValue();
			total += statistics.getAborts(AbortType.LOCKED_ON_READ);
		}
		return total;
	}

	public static int getNumAbortsLockedBeforeRead() {
		int total = 0;
		for (Map.Entry<Integer, Statistics> entry : statsMap.entrySet()) {
			Statistics statistics = entry.getValue();
			total += statistics.getAborts(AbortType.LOCKED_BEFORE_READ);
		}
		return total;
	}

	public static int getNumAbortsLockedBeforeElasticRead() {
		int total = 0;
		for (Map.Entry<Integer, Statistics> entry : statsMap.entrySet()) {
			Statistics statistics = entry.getValue();
			total += statistics.getAborts(AbortType.LOCKED_BEFORE_ELASTIC_READ);
		}
		return total;
	}

	public static int getNumAbortsInvalidCommit() {
		int total = 0;
		for (Map.Entry<Integer, Statistics> entry : statsMap.entrySet()) {
			Statistics statistics = entry.getValue();
			total += statistics.getAborts(AbortType.INVALID_COMMIT);
		}
		return total;
	}

	public static int getNumAbortsInvalidSnapshot() {
		int total = 0;
		for (Map.Entry<Integer, Statistics> entry : statsMap.entrySet()) {
			Statistics statistics = entry.getValue();
			total += statistics.getAborts(AbortType.INVALID_SNAPSHOT);
		}
		return total;
	}

	// public static String getAbortPer() {
	// int abortsSum = 0, totalStarts = 0;
	// for (Map.Entry<Integer, Statistics> entry : statsMap.entrySet()) {
	// Statistics statistics = entry.getValue();
	// abortsSum += statistics.getAborts(AbortType.ALL);
	// }

	public static String getTotalAbortsPercentage(AbortType type) {
		int abortsSum = 0, totalStarts = 0;
		for (Map.Entry<Integer, Statistics> entry : statsMap.entrySet()) {
			Statistics statistics = entry.getValue();
			if (type.equals(AbortType.ALL)) {
				abortsSum += statistics.getAborts(AbortType.ALL);
			} else if (type.equals(AbortType.BETWEEN_SUCCESSIVE_READS)) {
				abortsSum += statistics
						.getAborts(AbortType.BETWEEN_SUCCESSIVE_READS);
			} else if (type.equals(AbortType.BETWEEN_READ_AND_WRITE)) {
				abortsSum += statistics
						.getAborts(AbortType.BETWEEN_READ_AND_WRITE);
			} else if (type.equals(AbortType.EXTEND_ON_READ)) {
				abortsSum += statistics.getAborts(AbortType.EXTEND_ON_READ);
			} else if (type.equals(AbortType.WRITE_AFTER_READ)) {
				abortsSum += statistics.getAborts(AbortType.WRITE_AFTER_READ);
			} else if (type.equals(AbortType.LOCKED_ON_WRITE)) {
				abortsSum += statistics.getAborts(AbortType.LOCKED_ON_WRITE);
			} else if (type.equals(AbortType.LOCKED_ON_READ)) {
				abortsSum += statistics.getAborts(AbortType.LOCKED_ON_READ);
			} else if (type.equals(AbortType.LOCKED_BEFORE_READ)) {
				abortsSum += statistics.getAborts(AbortType.LOCKED_BEFORE_READ);
			} else if (type.equals(AbortType.LOCKED_BEFORE_ELASTIC_READ)) {
				abortsSum += statistics
						.getAborts(AbortType.LOCKED_BEFORE_ELASTIC_READ);
			} else if (type.equals(AbortType.INVALID_COMMIT)) {
				abortsSum += statistics.getAborts(AbortType.INVALID_COMMIT);
			} else if (type.equals(AbortType.INVALID_SNAPSHOT)) {
				abortsSum += statistics.getAborts(AbortType.INVALID_SNAPSHOT);
			}
			totalStarts += statistics.starts;
		}
		double result = percentage(abortsSum, totalStarts);
		return formatDouble(result);
	}

	public static String getTotalCommitsPercentage(CommitType type) {
		int commitsSum = 0, totalStarts = 0;
		for (Map.Entry<Integer, Statistics> entry : statsMap.entrySet()) {
			Statistics statistics = entry.getValue();
			if (type.equals(CommitType.ALL)) {
				commitsSum += statistics.getCommits(CommitType.ALL);
			} else if (type.equals(CommitType.READONLY)) {
				commitsSum += statistics.getCommits(CommitType.READONLY);
			} else if (type.equals(CommitType.ELASTIC)) {
				commitsSum += statistics.getCommits(CommitType.ELASTIC);
			} else if (type.equals(CommitType.UPDATE)) {
				commitsSum += statistics.getCommits(CommitType.UPDATE);
			}
			totalStarts += statistics.starts;
		}
		double result = percentage(commitsSum, totalStarts);
		return formatDouble(result);
	}

	public static String getTotalAvgReadSetSize() {
		double sumOfAverages = 0;
		for (Map.Entry<Integer, Statistics> entry : statsMap.entrySet()) {
			Statistics statistics = entry.getValue();
			sumOfAverages += statistics.getAvgReadSetSizeOnCommit();
		}
		return formatDouble(average(sumOfAverages, statsMap.size()));
	}

	public static String getTotalAvgWriteSetSize() {
		double sumOfAverages = 0;
		for (Map.Entry<Integer, Statistics> entry : statsMap.entrySet()) {
			Statistics statistics = entry.getValue();
			sumOfAverages += statistics.getAvgWriteSetSizeOnCommit();
		}
		return formatDouble(average(sumOfAverages, statsMap.size()));
	}

	public static String getTotalAvgCommitingTxTime() {
		int sum = 0;
		for (Map.Entry<Integer, Statistics> entry : statsMap.entrySet()) {
			Statistics statistics = entry.getValue();
			sum += statistics.txDurationSum;
		}
		double avgTimeInMS = 1000 * average(sum, getTotalCommits());
		return formatDouble(avgTimeInMS);
	}

	public static int getSumCommitingTxTime() {
		int sum = 0;
		for (Map.Entry<Integer, Statistics> entry : statsMap.entrySet()) {
			Statistics statistics = entry.getValue();
			sum += statistics.txDurationSum;
		}
		return sum * 1000;
	}

	public static int getTotalReadsInROPrefix() {
		int sum = 0;
		for (Map.Entry<Integer, Statistics> entry : statsMap.entrySet()) {
			Statistics statistics = entry.getValue();
			sum += statistics.readsInROPrefix;
		}
		return sum;
	}

	public static int getTotalElasticReads() {
		int sum = 0;
		for (Map.Entry<Integer, Statistics> entry : statsMap.entrySet()) {
			Statistics statistics = entry.getValue();
			sum += statistics.elasticReads;
		}
		return sum;
	}

	public static String getDetailedStatistics() {
		StringBuilder sb = new StringBuilder("");
		addStat("Starts                        ", getTotalStarts(), sb);
		addStat("Commits                       ", getTotalCommits(), sb);
		addStat("Commits (%)                   ",
				getTotalCommitsPercentage(CommitType.ALL), sb);
		addStat("  |--read only  (%)           ",
				getTotalCommitsPercentage(CommitType.READONLY), sb);
		addStat("  |--elastic (%)              ",
				getTotalCommitsPercentage(CommitType.ELASTIC), sb);
		addStat("  |--update (%)               ",
				getTotalCommitsPercentage(CommitType.UPDATE), sb);
		addStat("Aborts                        ", getTotalAborts(), sb);
		addStat("Aborts (%)                    ",
				getTotalAbortsPercentage(AbortType.ALL), sb);
		addStat("  |--between succ. reads  (%) ",
				getTotalAbortsPercentage(AbortType.BETWEEN_SUCCESSIVE_READS),
				sb);
		addStat("  |--between read & write (%) ",
				getTotalAbortsPercentage(AbortType.BETWEEN_READ_AND_WRITE), sb);
		addStat("  |--extend upon read     (%) ",
				getTotalAbortsPercentage(AbortType.EXTEND_ON_READ), sb);
		addStat("  |--write after read     (%) ",
				getTotalAbortsPercentage(AbortType.WRITE_AFTER_READ), sb);
		addStat("  |--locked on write      (%) ",
				getTotalAbortsPercentage(AbortType.LOCKED_ON_WRITE), sb);
		addStat("  |--locked on read       (%) ",
				getTotalAbortsPercentage(AbortType.LOCKED_ON_READ), sb);
		addStat("  |--locked before rread  (%) ",
				getTotalAbortsPercentage(AbortType.LOCKED_BEFORE_READ), sb);
		addStat("  |--locked before eread  (%) ",
				getTotalAbortsPercentage(AbortType.LOCKED_BEFORE_ELASTIC_READ),
				sb);
		addStat("  |--invalid commit       (%) ",
				getTotalAbortsPercentage(AbortType.INVALID_COMMIT), sb);
		addStat("  |--invalid snapshot     (%) ",
				getTotalAbortsPercentage(AbortType.INVALID_SNAPSHOT), sb);
		addStat("Avg. read set size            ", getTotalAvgReadSetSize(), sb);
		addStat("Avg. write set size           ", getTotalAvgWriteSetSize(), sb);
		addStat("Avg. commiting TX time (us)   ", getTotalAvgCommitingTxTime(),
				sb);
		addStat("Number of elastic reads       ", getTotalElasticReads(), sb);
		addStat("Number of reads in RO prefix  ", getTotalReadsInROPrefix(), sb);
		// getTxAttemptsHistogram(sb);
		return sb.toString();
	}

	/*
	 * private static void getTxAttemptsHistogram(StringBuilder sb) { int[]
	 * totalTxDurationHistCounters = new int[txAttemptsHistBins.length]; for
	 * (Map.Entry<Integer, Statistics> entry : statsMap.entrySet()) { Statistics
	 * statistics = entry.getValue(); for (int i=0;
	 * i<totalTxDurationHistCounters.length; i++) {
	 * totalTxDurationHistCounters[i] += statistics.txAttemptsHistCounters[i]; }
	 * } int totalCommits = getTotalCommits(); double[] totalTxDurationHist =
	 * new double[txAttemptsHistBins.length]; for (int i=0;
	 * i<totalTxDurationHist.length; i++) { totalTxDurationHist[i] =
	 * percentage(totalTxDurationHistCounters[i], totalCommits); }
	 * 
	 * sb.append("\n  Transaction Attempts Histogram:\n"); for (int i=0;
	 * i<totalTxDurationHist.length; i++) { sb.append("  # attempts <= ");
	 * sb.append(txAttemptsHistBins[i]); sb.append(": ");
	 * sb.append(formatDouble(totalTxDurationHist[i])); sb.append("%\n"); } }
	 */

	private static void addStat(String title, Object value, StringBuilder sb) {
		sb.append("  ");
		sb.append(title);
		sb.append(": ");
		sb.append(value);
		sb.append("\n");
	}

	private int starts = 0;
	private int abortsBetweenSuccessiveReads = 0;
	private int abortsBetweenReadAndWrite = 0;
	private int abortsExtendOnRead = 0;
	private int abortsWriteAfterRead = 0;
	private int abortsLockedOnWrite = 0;
	private int abortsLockedBeforeRead = 0;
	private int abortsLockedBeforeElasticRead = 0;
	private int abortsLockedOnRead = 0;
	private int abortsInvalidCommit = 0;
	private int abortsInvalidSnapshot = 0;

	private int commitReadOnly = 0;
	private int commitElastic = 0;
	private int commitUpdate = 0;

	private int commits = 0;
	private int aborts = 0;

	private long startTime;
	private long txDurationSum = 0;

	private int readsInROPrefix = 0;
	private int elasticReads = 0;

	private int[] txAttemptsHistCounters;

	private int readSetSizeOnCommitSum = 0;
	private int readSetSizeOnCommitCounter = 0;
	private int writeSetSizeOnCommitSum = 0;
	private int writeSetSizeOnCommitCounter = 0;

	private int validations = 0;
	private int validationReadBack = 0;
	private int validationReadSet = 0;
	private int validationCommitBack = 0;
	private int validationCommitRS = 0;

	public Statistics(int threadId) {
		statsMap.put(threadId, this);
		txAttemptsHistCounters = new int[txAttemptsHistBins.length];
	}

	public void reportTxStart() {
		this.starts++;
		this.startTime = System.currentTimeMillis();
	}

	public void reportValidation(ValidationType type) {
		if (type.equals(ValidationType.READ_BACK)) {
			this.validationReadBack++;
		} else if (type.equals(ValidationType.READ_SET)) {
			this.validationReadSet++;
		} else if (type.equals(ValidationType.COMMIT_BACK)) {
			this.validationCommitBack++;
		} else if (type.equals(ValidationType.COMMIT_RS)) {
			this.validationCommitRS++;
		}
		this.validations++;
	}

	public int getValidation(ValidationType type) {
		if (type.equals(ValidationType.ALL)) {
			return this.validations;
		} else if (type.equals(ValidationType.READ_BACK)) {
			return this.validationReadBack;
		} else if (type.equals(ValidationType.READ_SET)) {
			return this.validationReadSet;
		} else if (type.equals(ValidationType.COMMIT_BACK)) {
			return this.validationCommitBack;
		} else if (type.equals(ValidationType.COMMIT_RS)) {
			return this.validationCommitRS;
		}
		throw new IllegalArgumentException("ValidationType unrecognized "
				+ type);
	}

	public void reportAbort(AbortType type) {
		startTime = -1;
		if (type.equals(AbortType.BETWEEN_SUCCESSIVE_READS)) {
			this.abortsBetweenSuccessiveReads++;
		} else if (type.equals(AbortType.BETWEEN_READ_AND_WRITE)) {
			this.abortsBetweenReadAndWrite++;
		} else if (type.equals(AbortType.EXTEND_ON_READ)) {
			this.abortsExtendOnRead++;
		} else if (type.equals(AbortType.WRITE_AFTER_READ)) {
			this.abortsWriteAfterRead++;
		} else if (type.equals(AbortType.LOCKED_ON_WRITE)) {
			this.abortsLockedOnWrite++;
		} else if (type.equals(AbortType.LOCKED_BEFORE_READ)) {
			this.abortsLockedBeforeRead++;
		} else if (type.equals(AbortType.LOCKED_BEFORE_ELASTIC_READ)) {
			this.abortsLockedBeforeElasticRead++;
		} else if (type.equals(AbortType.LOCKED_ON_READ)) {
			this.abortsLockedOnRead++;
		} else if (type.equals(AbortType.INVALID_COMMIT)) {
			this.abortsInvalidCommit++;
		} else if (type.equals(AbortType.INVALID_SNAPSHOT)) {
			this.abortsInvalidSnapshot++;
		}
		this.aborts++;
	}

	public void reportCommit(CommitType type) {
		startTime = -1;
		if (type.equals(CommitType.READONLY)) {
			this.commitReadOnly++;
		} else if (type.equals(CommitType.ELASTIC)) {
			this.commitElastic++;
		} else if (type.equals(CommitType.UPDATE)) {
			this.commitUpdate++;
		}
		this.commits++;
	}

	public void reportCommit(int attempts) {
		this.commits++;
		if (startTime != -1) {
			long txDuration = System.currentTimeMillis() - startTime;
			txDurationSum += txDuration;
			for (int i = 0; i < txAttemptsHistBins.length; i++) {
				int limit = txAttemptsHistBins[i];
				if (attempts <= limit) {
					txAttemptsHistCounters[i]++;
					break;
				}
			}
		}
	}

	public void reportElasticReads() {
		elasticReads += 1;
	}

	public void reportInROPrefix() {
		readsInROPrefix += 1;
	}

	public int getAborts(AbortType type) {
		if (type.equals(AbortType.ALL)) {
			return this.aborts;
		} else if (type.equals(AbortType.BETWEEN_SUCCESSIVE_READS)) {
			return abortsBetweenSuccessiveReads;
		} else if (type.equals(AbortType.BETWEEN_READ_AND_WRITE)) {
			return abortsBetweenReadAndWrite;
		} else if (type.equals(AbortType.EXTEND_ON_READ)) {
			return abortsExtendOnRead;
		} else if (type.equals(AbortType.WRITE_AFTER_READ)) {
			return abortsWriteAfterRead;
		} else if (type.equals(AbortType.LOCKED_ON_WRITE)) {
			return abortsLockedOnWrite;
		} else if (type.equals(AbortType.LOCKED_BEFORE_ELASTIC_READ)) {
			return abortsLockedBeforeElasticRead;
		} else if (type.equals(AbortType.LOCKED_BEFORE_READ)) {
			return abortsLockedBeforeRead;
		} else if (type.equals(AbortType.LOCKED_ON_READ)) {
			return abortsLockedOnRead;
		} else if (type.equals(AbortType.INVALID_COMMIT)) {
			return abortsInvalidCommit;
		} else if (type.equals(AbortType.INVALID_SNAPSHOT)) {
			return abortsInvalidSnapshot;
		}
		throw new IllegalArgumentException("AbortType unrecognized " + type);
	}

	public int getCommits(CommitType type) {
		if (type.equals(AbortType.ALL)) {
			return this.commits;
		} else if (type.equals(CommitType.READONLY)) {
			return commitReadOnly;
		} else if (type.equals(CommitType.ELASTIC)) {
			return commitElastic;
		} else if (type.equals(CommitType.UPDATE)) {
			return commitUpdate;
		}
		throw new IllegalArgumentException("CommitType unrecognized " + type);
	}

	public void reportOnCommit(int rsSize, int wsSize) {
		readSetSizeOnCommitSum += rsSize;
		readSetSizeOnCommitCounter++;
		if (wsSize > 0) {
			// We only consider writing transactions
			// when we calculate average write-set size
			writeSetSizeOnCommitSum += wsSize;
			writeSetSizeOnCommitCounter++;
		}
	}

	public void resetSizeStats() {
		readSetSizeOnCommitSum = 0;
		readSetSizeOnCommitCounter = 0;
		writeSetSizeOnCommitSum = 0;
		writeSetSizeOnCommitCounter = 0;
	}

	public static double getSumReadSetSize() {
		double sumOfAverages = 0;
		for (Map.Entry<Integer, Statistics> entry : statsMap.entrySet()) {
			Statistics statistics = entry.getValue();
			sumOfAverages += statistics.getAvgReadSetSizeOnCommit();
		}
		return sumOfAverages;
	}

	public static double getSumWriteSetSize() {
		double sumOfAverages = 0;
		for (Map.Entry<Integer, Statistics> entry : statsMap.entrySet()) {
			Statistics statistics = entry.getValue();
			sumOfAverages += statistics.getAvgWriteSetSizeOnCommit();
		}
		return sumOfAverages;
	}

	public static int getStatSize() {
		return statsMap.size();
	}

	public double getAvgReadSetSizeOnCommit() {
		return average(readSetSizeOnCommitSum, readSetSizeOnCommitCounter);
	}

	public double getAvgWriteSetSizeOnCommit() {
		return average(writeSetSizeOnCommitSum, writeSetSizeOnCommitCounter);
	}

	private static double percentage(int p, int w) {
		return (double) p / w * 100;
	}

	private static double average(double sum, double n) {
		return sum / n;
	}

	private static String formatDouble(double result) {
		Formatter formatter = new Formatter(Locale.US);
		return formatter.format("%.2f", result).out().toString();
	}

}
