/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* This file is part of the GtkHTML library.

   Copyright (C) 1997 Martin Jones (mjones@kde.org)
   Copyright (C) 1997 Torben Weis (weis@kde.org)
   Copyright (C) 1999 Anders Carlsson (andersca@gnu.org)
   Copyright (C) 2000 Helix Code, Inc.
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#include "htmlobject.h"
#include "htmltable.h"
#include "htmlsearch.h"
#include <string.h>


#define COLUMN_INFO(table, i)				\
	(g_array_index (table->colInfo, ColumnInfo, i))

#define COLUMN_TYPE(table, i)				\
	(g_array_index (table->colType, ColumnType, i))

#define COLUMN_POS(table, i)				\
	(g_array_index (table->columnPos, gint, i))

#define COLUMN_PREF_POS(table, i)				\
	(g_array_index (table->columnPrefPos, gint, i))

#define COLUMN_SPAN(table, i)				\
	(g_array_index (table->colSpan, gint, i))

#define COLUMN_OPT(table, i)				\
	(g_array_index (table->columnOpt, gint, i))

#define ROW_HEIGHT(table, i)				\
	(g_array_index (table->rowHeights, gint, i))


HTMLTableClass html_table_class;
static HTMLObjectClass *parent_class = NULL;

static void alloc_cell (HTMLTable *table, gint r, gint c);
static void set_cell   (HTMLTable *table, gint r, gint c, HTMLTableCell *cell);
static void do_cspan   (HTMLTable *table, gint row, gint col, HTMLTableCell *cell);
static void do_rspan   (HTMLTable *table, gint row);



static gboolean
invalid_cell (HTMLTable *table, gint r, gint c)
{
	return (table->cells[r][c] == NULL
		|| (c < table->totalCols - 1 && table->cells[r][c] == table->cells[r][c+1])
		|| (r < table->totalRows - 1 && table->cells[r][c] == table->cells[r+1][c]));
}

/* HTMLObject methods.  */

static void
destroy (HTMLObject *o)
{
	HTMLTable *table = HTML_TABLE (o);
	HTMLTableCell *cell;
	guint r, c;

	for (r = 0; r < table->allocRows; r++) {
		for (c = 0; c < table->totalCols; c++) {
			if ((cell = table->cells[r][c]) == 0)
				continue;
			if (c < table->totalCols - 1 &&
			    cell == table->cells[r][c + 1])
				continue;
			if (r < table->totalRows - 1 &&
			    cell == table->cells[r + 1][c])
				continue;

			html_object_destroy (HTML_OBJECT (cell));
		}
		g_free (table->cells [r]);
	}
	g_free (table->cells);

	g_array_free (table->colInfo, TRUE);
	g_array_free (table->colType, TRUE);
	g_array_free (table->columnPos, TRUE);
	g_array_free (table->columnPrefPos, TRUE);
	g_array_free (table->colSpan, TRUE);
	g_array_free (table->columnOpt, TRUE);
	g_array_free (table->rowHeights, TRUE);

	HTML_OBJECT_CLASS (parent_class)->destroy (o);
}

static void
copy (HTMLObject *self,
      HTMLObject *dest)
{
	(* HTML_OBJECT_CLASS (parent_class)->copy) (self, dest);

	/* FIXME TODO*/
	g_warning ("HTMLTable::copy is broken.");
}

static gboolean
is_container (HTMLObject *object)
{
	return TRUE;
}

static void
forall (HTMLObject *self,
	HTMLObjectForallFunc func,
	gpointer data)
{
	HTMLTableCell *cell;
	HTMLTable *table;
	guint r, c;

	table = HTML_TABLE (self);

	/* FIXME rowspan/colspan cells?  */

	for (r = 0; r < table->totalRows; r++) {
		for (c = 0; c < table->totalCols; c++) {
			cell = table->cells[r][c];

			if (cell == NULL)
				continue;
			if (c < table->totalCols - 1 && cell == table->cells[r][c + 1])
				continue;
			if (r < table->totalRows - 1 && table->cells[r + 1][c] == cell)
				continue;

			html_object_forall (HTML_OBJECT (cell), func, data);
		}
	}
}



static void
previous_rows_do_cspan (HTMLTable *table, gint c)
{
	gint i;
	if (c)
		for (i=0; i < table->totalRows - 1; i++)
			if (table->cells [i][c - 1])
				do_cspan (table, i, c, table->cells [i][c - 1]);
}

static void
expand_columns (HTMLTable *table, gint num)
{
	gint r;

	for (r = 0; r < table->allocRows; r++) {
		table->cells [r] = g_renew (HTMLTableCell *, table->cells [r], table->totalCols + num);
		memset (table->cells [r] + table->totalCols, 0, num * sizeof (HTMLTableCell *));
	}
	table->totalCols += num;
}

static void
inc_columns (HTMLTable *table, gint num)
{
	expand_columns (table, num);
	previous_rows_do_cspan (table, table->totalCols - num);
}

static void
expand_rows (HTMLTable *table, gint num)
{
	gint r;

	table->cells = g_renew (HTMLTableCell **, table->cells, table->allocRows + num);

	for (r = table->allocRows; r < table->allocRows + num; r++) {
		table->cells [r] = g_new (HTMLTableCell *, table->totalCols);
		memset (table->cells [r], 0, table->totalCols * sizeof (HTMLTableCell *));
	}

	table->allocRows += num;
}

static void
inc_rows (HTMLTable *table, gint num)
{
	if (table->totalRows + num > table->allocRows)
		expand_rows (table, num + MAX (10, table->allocRows >> 2));
	table->totalRows += num;
	if (table->totalRows - num > 0)
		do_rspan (table, table->totalRows - num);
}

static void
calc_col_info (HTMLTable *table,
	       HTMLPainter *painter)
{
	RowInfo *rowInfo;
	gint pixel_size;
	gint r, c;
	gint borderExtra;
	gint i, j, totalRowInfos;

	pixel_size = html_painter_get_pixel_size (painter);

	if (table->border == 0)
		borderExtra = 0;
	else
		borderExtra = 1;

	/* Allocate some memory for column info */
	g_array_set_size (table->colInfo, table->totalCols * 2);
	rowInfo = g_new (RowInfo, table->totalRows);
	table->totalColumnInfos = 0;

	for (r = 0; r < table->totalRows; r++) {
		rowInfo[r].entry = g_new0 (gint, table->totalCols);
		rowInfo[r].nrEntries = 0;
		for (c = 0; c < table->totalCols; c++) {
			HTMLTableCell *cell = table->cells[r][c];
			gint min_size;
			gint pref_size;
			gint colInfoIndex;
			ColumnType col_type;

			if (cell == 0)
				continue;
			if (c > 0 && table->cells[r][c-1] == cell)
				continue;
			if (r > 0 && table->cells[r-1][c] == cell)
				continue;

			/* calculate minimum size */
			min_size = html_object_calc_min_width (HTML_OBJECT (cell), painter);

			min_size += (table->padding * 2
				     + table->spacing
				     + borderExtra) * pixel_size;

			/* calculate preferred pos */
			if (HTML_OBJECT (cell)->percent > 0) {
				/* The cast to `gdouble' is to avoid overflow (eg. when
				   printing).  */
				pref_size = ((gdouble) HTML_OBJECT (table)->max_width
					     * HTML_OBJECT (cell)->percent / 100);
				pref_size += pixel_size * (table->padding * 2
							   + table->spacing
							   + borderExtra);
				col_type = COLUMN_TYPE_PERCENT;
			} else if (HTML_OBJECT (cell)->flags
				   & HTML_OBJECT_FLAG_FIXEDWIDTH) {
				pref_size = HTML_OBJECT (cell)->width;
				pref_size += pixel_size * (table->padding * 2
							   + table->spacing
							   + borderExtra);
				col_type = COLUMN_TYPE_FIXED;
			} else {
				pref_size = html_object_calc_preferred_width (HTML_OBJECT (cell),
									      painter);
				pref_size += pixel_size * (table->padding * 2
							   + table->spacing
							   + borderExtra);
				col_type = COLUMN_TYPE_VARIABLE;
			}

			colInfoIndex = html_table_add_col_info (table, c, cell->cspan,
								min_size, pref_size,
								HTML_OBJECT (table)->max_width,
								col_type);

			rowInfo[r].entry[rowInfo[r].nrEntries++] = colInfoIndex;
		}
	}

	/* Remove redundant rows */
	totalRowInfos = 1;
	for (i = 1; i < table->totalRows; i++) {
		gboolean unique = TRUE;
		for (j = 0; (j < totalRowInfos) && (unique == TRUE); j++) {
			gint k;
			if (rowInfo[i].nrEntries == rowInfo[j].nrEntries)
				unique = FALSE;
			else {
				gboolean match = TRUE;
				k = rowInfo[i].nrEntries;
				while (k--) {
					if (rowInfo[i].entry[k] != 
					    rowInfo[j].entry[k]) {
						match = FALSE;
						break;
					}
				}
				
				if (match)
					unique = FALSE;
			}
		}
		if (!unique) {
			g_free (rowInfo[i].entry);
			rowInfo[i].entry = NULL;
		} else {
			if (totalRowInfos != i) {
				rowInfo[totalRowInfos].entry = rowInfo[i].entry;
				rowInfo[totalRowInfos].nrEntries = rowInfo[i].nrEntries;
			}
			totalRowInfos++;
		}
	}

	/* Calculate pref width and min width for each row */

	table->_minWidth = 0;
	table->_prefWidth = 0;
	for (i = 0; i < totalRowInfos; i++) {
		gint min = 0;
		gint pref = 0;
		gint j;

		for (j = 0; j < rowInfo[i].nrEntries; j++) {
			gint index;

			index = rowInfo[i].entry[j];
			min += COLUMN_INFO (table, index).minSize;
			pref += COLUMN_INFO (table, index).prefSize;
		}

		rowInfo[i].minSize = min;
		rowInfo[i].prefSize = pref;

		if (table->_minWidth < min)
			table->_minWidth = min;

		if (table->_prefWidth < pref)
			table->_prefWidth = pref;
	       
	}

	if (HTML_OBJECT (table)->flags & HTML_OBJECT_FLAG_FIXEDWIDTH) {
		/* Our minimum width is at least our fixed width */
		if (table->specified_width > table->_minWidth)
			table->_minWidth = table->specified_width;
	}

	if (table->_minWidth > table->_prefWidth)
		table->_prefWidth = table->_minWidth;

	for (r=0; r<totalRowInfos; r++) {
		g_free (rowInfo [r].entry);
	}
	g_free (rowInfo);
}

static gint
scale_selected_columns (HTMLTable *table, gint c_start, gint c_end,
			gint tooAdd, gboolean *selected)
{
	gint c, c1;
	gint numSelected = 0;
	gint addSize, left;

	if (tooAdd <= 0)
		return tooAdd;
	
	for (c = c_start; c <= c_end; c++) {
		if (selected[c])
			numSelected++;
	}
	if (numSelected < 1) 
		return tooAdd;

	addSize = tooAdd / numSelected;
	left = tooAdd - addSize * numSelected;
	
	for (c = c_start; c <= c_end; c++) {
		if (!selected[c])
			continue;
		tooAdd -= addSize;
		for (c1 = c + 1; c1 <= (gint)table->totalCols; c1++) {
			COLUMN_OPT (table, c1) += addSize;
			if (left)
				COLUMN_OPT (table, c1)++;
		}
		if (left) {
			tooAdd--;
			left--;
		}
	}
	return tooAdd;
}

static void
scale_columns (HTMLTable *table, HTMLPainter *painter,
	       gint c_start, gint c_end, gint tooAdd)
{
	gint r, c;
	gint colspan;
	gint addSize;
	gint minWidth, prefWidth;
	gint totalAllowed, totalRequested;
	gint borderExtra;

	gint tableWidth;
	gint pixel_size;
	gint *prefColumnWidth;
	gboolean *fixedCol;
	gboolean *percentCol;
	gboolean *variableCol;

	pixel_size = html_painter_get_pixel_size (painter);
	tableWidth = HTML_OBJECT (table)->width  - table->border * pixel_size;

	if (table->border == 0)
		borderExtra = 0;
	else
		borderExtra = 1;

	/* Satisfy fixed width cells */
	for (colspan = 0; colspan <= 1; colspan++) {
		for (r = 0; r < table->totalRows; r++) {
			for (c = c_start; c <= c_end; c++) {
				HTMLTableCell *cell = table->cells[r][c];

				if (cell == 0)
					continue;

				if (r < table->totalRows - 1 && 
				    table->cells[r + 1][c] == cell)
					continue;

				/* COLUMN_TYPE_FIXED cells only */
				if (!(HTML_OBJECT(cell)->flags
				      & HTML_OBJECT_FLAG_FIXEDWIDTH))
					continue;

				if (colspan == 0) {
					/* colSpan == 1 */
					if (cell->cspan != 1)
						continue;
				}
				else {
					/* colSpan > 1 */
					if (cell->cspan <= 1)
						continue;
					if (c < table->totalCols - 1 &&
					    table->cells[r][c + 1] == cell)
						continue;
				}

				minWidth  = (COLUMN_OPT (table, c + 1) - COLUMN_OPT (table, cell->col));
				prefWidth = (cell->fixed_width
					     + pixel_size * (table->padding * 2
							     + table->spacing
							     + borderExtra));

				if (prefWidth <= minWidth)
					continue;
				
				addSize = prefWidth - minWidth;
				tooAdd -= addSize;

				if (colspan == 0) {
					gint c1;
					/* Just add this to the column size */
					for (c1 = c + 1; c1 <= table->totalCols; c1++)
						COLUMN_OPT (table, c1) += addSize;
				}
				else {
					/* Some end-conditions are required here to prevent looping */
					if (cell->col < c_start)
						continue;
					if ((cell->col == c_start) && (c == c_end))
						continue;

					/* Scale the columns covered by 'cell' first */
					scale_columns (table, painter, cell->col, c, addSize);
				}
			}
		}
	}

	/* Satisfy percentage width cells */
	for (r = 0; r < table->totalRows; r++) {
		totalRequested = 0;
		if (tooAdd <= 0) /* No space left! */
			return;
		
		/* first calculate how much we would like to add in this row */
		for (c = c_start; c <= c_end; c++) {
			HTMLTableCell *cell = table->cells[r][c];
			
			if (cell == 0)
				continue;
			if (r < table->totalRows - 1 &&
			    table->cells[r + 1][c] == cell)
				continue;
			if (c < table->totalCols - 1 &&
			    table->cells[r][c + 1] == cell)
				continue;

			/* Percentage cells only */
			if (HTML_OBJECT (cell)->percent <=0)
				continue;

			/* Only cells with a colspan which fits within
			   c_begin .. c_start */
			if (cell->cspan > 1) {
				gint c_b = c + 1 - cell->cspan;

				if (c_b < c_start)
					continue;
				if (c_b == c_start && c == c_end)
					continue;
			}

			minWidth = (COLUMN_OPT (table, c + 1)
				    - COLUMN_OPT (table, c + 1 - cell->cspan));

			/* The cast to `gdouble' is to avoid overflow
                           (eg. when printing).  */
			prefWidth = (((gdouble) tableWidth * HTML_OBJECT (cell)->percent) / 100
				     + pixel_size * (table->padding * 2
						     + table->spacing
						     + borderExtra));

			if (prefWidth <= minWidth)
				continue;

			totalRequested += prefWidth - minWidth;
		}

		if (totalRequested == 0) /* Nothing to do */
			continue;

		totalAllowed = tooAdd;

		/* Do the actual adjusting of the percentage cells */
		for (colspan = 0; colspan <= 1; colspan ++) {
			for (c = c_start; c <= c_end; c++) {
				HTMLTableCell *cell = table->cells[r][c];

				if (cell == 0)
					continue;
				if (c < table->totalCols - 1 &&
				    table->cells[r][c + 1] == cell)
					continue;
				if (r < table->totalRows - 1 &&
				    table->cells[r + 1][c] == cell)
					continue;

				/* Percentage cells only */
				if (HTML_OBJECT (cell)->percent <= 0)
					continue;

				/* Only cells with a colspan which fits
				   within c_begin .. c_start */
				if (cell->cspan > 1) {
					if (colspan == 0)
						continue;

					if (cell->col < c_start)
						continue;
					if ((cell->col == c_start) && (c == c_end))
						continue;
				}
				else {
					if (colspan != 0)
						continue;
				}

				minWidth = COLUMN_OPT (table, c + 1) - 
					COLUMN_OPT (table, cell->col);

				/* The cast to `gdouble' is to avoid overflow (eg. when
				   printing).  */
				prefWidth = (((gdouble) tableWidth
					      * HTML_OBJECT (cell)->percent) / 100
					     + pixel_size * (table->padding * 2
							     + table->spacing
							     + borderExtra));

				if (prefWidth <= minWidth)
					continue;

				addSize = prefWidth - minWidth;

				if (totalRequested > totalAllowed) { 
					/* We can't honour the request, scale it */
					/* The cast to `gdouble' is to avoid overflow (eg. when
					   printing).  */
					addSize = (((gdouble) addSize * totalAllowed)
						   / totalRequested);
					totalRequested -= prefWidth - minWidth;
					totalAllowed -= addSize;
				}

				tooAdd -= addSize;

				if (colspan == 0) {
					gint c1;

					/* Just add this to the column size */
					for (c1 = c + 1; c1 <= table->totalCols; c1++)
						COLUMN_OPT (table, c1) += addSize;
				}
				else {
					gint c_b = c + 1 - cell->cspan;

					/* Some end-conditions are required here
					   to prevent looping */
					if (c_b < c_start)
						continue;
					if ((c_b == c_start) && (c == c_end))
						continue;

					/* Scale the columns covered by 'cell'
                                           first */
					scale_columns (table, painter, c_b, c, addSize);
				}
			}
		}
	}

	totalRequested = 0;

	if (tooAdd <= 0) {
		return;
	}

	prefColumnWidth = g_new0 (int, table->totalCols);
	fixedCol = g_new0 (gboolean, table->totalCols);
	percentCol = g_new0 (gboolean, table->totalCols);
	variableCol = g_new0 (gboolean, table->totalCols);

	/* first calculate how much we would like to add in each column */
	for (c = c_start; c <= c_end; c++) {
		minWidth = COLUMN_OPT (table, c + 1) - COLUMN_OPT (table, c);
		prefColumnWidth [c] = minWidth;
		fixedCol[c] = FALSE;
		percentCol[c] = FALSE;
		variableCol[c] = TRUE;
		for (r = 0; r < table->totalRows; r++) {
			gint prefCellWidth;
			HTMLTableCell *cell = table->cells[r][c];

			if (cell == 0)
				continue;
			if (r < table->totalRows - 1 && table->cells[r + 1][c] == cell)
				continue;

			if (HTML_OBJECT(cell)->flags
			    & HTML_OBJECT_FLAG_FIXEDWIDTH) {
				/* fixed width */
				prefCellWidth = (HTML_OBJECT (cell)->width
						 + pixel_size * (table->padding * 2
								 + table->spacing
								 + borderExtra));
				fixedCol[c] = TRUE;
				variableCol[c] = FALSE;

			}
			else if (HTML_OBJECT (cell)->percent > 0) {
				/* percentage width */
				/* The cast to `gdouble' is to avoid overflow (eg. when
				   printing).  */
				prefCellWidth = (((gdouble) tableWidth
						  * HTML_OBJECT (cell)->percent) / 100
						 + pixel_size * (table->padding * 2
								 + table->spacing
								 + borderExtra));
				percentCol[c] = TRUE;
				variableCol[c] = FALSE;

			}
			else {
				/* variable width */
				prefCellWidth = (html_object_calc_preferred_width (HTML_OBJECT (cell),
										   painter)
						 + pixel_size * (table->padding * 2
								 + table->spacing
								 + borderExtra));
			}
			
			prefCellWidth = prefCellWidth / MIN (cell->cspan, table->totalCols - cell->col);

			if (prefCellWidth > prefColumnWidth [c])
				prefColumnWidth [c] = prefCellWidth;
		}

		if (prefColumnWidth [c] > minWidth) {
			totalRequested += (prefColumnWidth [c] - minWidth);
		}
		else {
			prefColumnWidth [c] = 0;
		}
	}

	if (totalRequested > 0) { /* Nothing to do */
		totalAllowed = tooAdd;

		/* Do the actual adjusting of the variable width cells */

		for (c = c_start; c <= c_end; c++) {
			gint c1;

			minWidth = COLUMN_OPT (table, c + 1) - COLUMN_OPT (table, c);
			prefWidth = prefColumnWidth [c];

			if (prefWidth <= minWidth || fixedCol[c] || percentCol[c])
				continue;

			addSize = prefWidth - minWidth;

			if (totalRequested > totalAllowed) {
				/* We can't honour the request, scale it */
				/* The cast to `gdouble' is to avoid overflow (eg. when
				   printing).  */
				addSize = ((gdouble) addSize * totalAllowed) / totalRequested;
				totalRequested -= prefWidth - minWidth;
				totalAllowed -= addSize;
			}

			tooAdd -= addSize;

			/* Just add this to the column size */
			for (c1 = c + 1; c1 <= table->totalCols; c1++)
				COLUMN_OPT (table, c1) += addSize;
		}
		
	}
	g_free (prefColumnWidth);

	/* Spread the remaining space equally across all variable columns */
	if (tooAdd > 0)
		tooAdd = scale_selected_columns (table, c_start, c_end,
						 tooAdd, variableCol);

	/* Spread the remaining space equally across all percentage columns */
	if (tooAdd > 0)
		tooAdd = scale_selected_columns (table, c_start, c_end,
						 tooAdd, percentCol);

	/* Still something left ... Change fixed columns */
	if (tooAdd > 0)
		tooAdd = scale_selected_columns (table, c_start, c_end,
						 tooAdd, fixedCol);

	g_free (fixedCol);
	g_free (percentCol);
	g_free (variableCol);
}

/* Both the minimum and preferred column sizes are calculated here.
   The hard part is choosing the actual sizes based on these two. */
static void
calc_column_widths (HTMLTable *table,
		    HTMLPainter *painter)
{
	gint r, c, i;
	gint borderExtra = (table->border == 0) ? 0 : 1;
	gint pixel_size;

	pixel_size = html_painter_get_pixel_size (painter);

	g_array_set_size (table->colType, table->totalCols + 1);
	for (i = 0; i < table->colType->len; i++)
		COLUMN_TYPE (table, i) = COLUMN_TYPE_VARIABLE;

	g_array_set_size (table->columnPos, table->totalCols + 1);
	COLUMN_POS (table, 0) = pixel_size * (table->border + table->spacing);
	
	g_array_set_size (table->columnPrefPos, table->totalCols + 1);
	COLUMN_PREF_POS (table, 0) = pixel_size * (table->border + table->spacing);

	g_array_set_size (table->colSpan, table->totalCols + 1);
	for (i = 0; i < table->colSpan->len; i++)
		COLUMN_SPAN (table, i) = 1;

	for (c = 0; c < table->totalCols; c++) {
		COLUMN_POS (table, c + 1) = 0;
		COLUMN_PREF_POS (table, c + 1) = 0;

		for (r = 0; r < table->totalRows; r++) {
			HTMLTableCell *cell = table->cells[r][c];
			gint colPos;

			if (cell == 0)
				continue;

			if (c < table->totalCols - 1 && 
			    table->cells[r][c + 1] == cell)
				continue;
			if (r < table->totalRows - 1 && 
			    table->cells[r + 1][c] == cell)
				continue;

			/* calculate minimum pos */
			colPos = (COLUMN_POS (table, cell->col)
				  + html_object_calc_min_width (HTML_OBJECT (cell), painter)
				  + pixel_size * (table->padding * 2
						  + table->spacing
						  + borderExtra));
			
			if (COLUMN_POS (table, c + 1) < colPos)
				COLUMN_POS (table, c + 1) = colPos;

			if (COLUMN_TYPE (table, c + 1) != COLUMN_TYPE_VARIABLE) {
				continue;
			}

			/* Calculate preferred pos */
			if (HTML_OBJECT (cell)->percent > 0) {
				/* The cast to `gdouble' is to avoid overflow (eg. when
				   printing).  */
				colPos = (COLUMN_PREF_POS (table, cell->col)
					  + ((gdouble) HTML_OBJECT (table)->max_width
					     * HTML_OBJECT (cell)->percent / 100)
					  + pixel_size * (table->padding * 2
							  + table->spacing
							  + borderExtra));
				COLUMN_TYPE (table, c + 1) = COLUMN_TYPE_PERCENT;
				COLUMN_SPAN (table, c + 1) = cell->cspan;
				COLUMN_PREF_POS (table, c + 1) = colPos;
			}
			else if (HTML_OBJECT (cell)->flags
				 & HTML_OBJECT_FLAG_FIXEDWIDTH) {
				colPos = (COLUMN_PREF_POS (table, cell->col)
					  + HTML_OBJECT (cell)->width
					  + pixel_size * (table->padding * 2
							  + table->spacing
							  + borderExtra));
				COLUMN_TYPE (table, c + 1) = COLUMN_TYPE_FIXED;
				COLUMN_SPAN (table, c + 1) = cell->cspan;
				COLUMN_PREF_POS (table, c + 1) = colPos;
			}
			else {
				colPos = html_object_calc_preferred_width (HTML_OBJECT (cell),
									   painter);
				colPos += (COLUMN_PREF_POS (table, cell->col)
					   + pixel_size * (table->padding * 2
							   + table->spacing
							   + borderExtra));

				if (COLUMN_PREF_POS (table, c + 1) < colPos)
					COLUMN_PREF_POS (table, c + 1) = colPos;

			}
			
			if (COLUMN_PREF_POS (table, c + 1) < COLUMN_POS (table, c + 1))
				COLUMN_PREF_POS (table, c + 1) = COLUMN_POS (table, c + 1);
		}

		if (COLUMN_POS (table, c + 1) <= COLUMN_POS (table, c))
			COLUMN_POS (table, c + 1) = COLUMN_POS (table, c);

		if (COLUMN_PREF_POS (table, c + 1) <= COLUMN_PREF_POS (table, c))
			COLUMN_PREF_POS (table, c + 1) = COLUMN_PREF_POS (table, c) + 1;
	}
}

static void
calc_row_heights (HTMLTable *table,
		  HTMLPainter *painter)
{
	gint r, c;
	gint rowPos, borderExtra = table->border ? 1: 0;
	HTMLTableCell *cell;
	gint pixel_size;

	pixel_size = html_painter_get_pixel_size (painter);

	g_array_set_size (table->rowHeights, table->totalRows + 1);
	ROW_HEIGHT (table, 0) = pixel_size *(table->border + table->spacing);

	for (r = 0; r < table->totalRows; r++) {
		ROW_HEIGHT (table, r + 1) = 0;
		for (c = 0; c < table->totalCols; c++) {
			if ((cell = table->cells[r][c]) == 0)
				continue;
			if (c < table->totalCols - 1 &&
			    cell == table->cells[r][c + 1])
				continue;
			if (r < table->totalRows - 1 &&
			    table->cells[r + 1][c] == cell)
				continue;

			rowPos = (ROW_HEIGHT (table, cell->row) + 
				  HTML_OBJECT (cell)->ascent +
				  HTML_OBJECT (cell)->descent
				  + (pixel_size * (table->spacing
						   + borderExtra)));

			if (rowPos > ROW_HEIGHT (table, r + 1))
				ROW_HEIGHT (table, r + 1) = rowPos;
		}
		
		if (ROW_HEIGHT (table, r + 1) < ROW_HEIGHT (table, r))
			ROW_HEIGHT (table, r + 1) = ROW_HEIGHT (table, r);
	}
}

static void
optimize_cell_width (HTMLTable *table,
		     HTMLPainter *painter,
		     gint width)
{
	gint addSize;

	addSize = 0;
	if (width > COLUMN_POS (table, table->totalCols)) {
		/* We have some space to spare */
		addSize = width - COLUMN_POS (table, table->totalCols) + html_painter_get_pixel_size (painter) * table->border;

		if ((HTML_OBJECT (table)->percent <= 0)
		    && (!(HTML_OBJECT (table)->flags
			  & HTML_OBJECT_FLAG_FIXEDWIDTH))) {

			/* COLUMN_TYPE_VARIABLE width table */
			if (COLUMN_PREF_POS (table, table->totalCols) < width) {
				/* Don't scale beyond preferred width */
				addSize = (COLUMN_PREF_POS (table, table->totalCols)
					   - COLUMN_POS (table, table->totalCols));
			}
		}
	}
	
	if (addSize > 0)
		scale_columns (table, painter, 0, table->totalCols - 1, addSize);
}

static void
do_cspan (HTMLTable *table, gint row, gint col, HTMLTableCell *cell)
{
	gint i;

	g_assert (cell);
	g_assert (cell->col <= col);

	for (i=col - cell->col; i < cell->cspan && cell->col + i < table->totalCols; i++)
		set_cell (table, row, cell->col + i, cell);
}

static void
prev_col_do_cspan (HTMLTable *table, gint row)
{
	g_assert (row >= 0);

	/* add previous column cell which has cspan > 1 */
	while (table->col < table->totalCols && table->cells [row][table->col] != 0) {
		alloc_cell (table, row, table->col + table->cells [row][table->col]->cspan);
		do_cspan (table, row, table->col + 1, table->cells [row][table->col]);
		table->col += (table->cells [row][table->col])->cspan;
	}
}

static void
do_rspan (HTMLTable *table, gint row)
{
	gint i;

	g_assert (row > 0);

	for (i=0; i<table->totalCols; i++)
		if (table->cells [row - 1][i]
		    && (table->cells [row - 1][i])->row + (table->cells [row - 1][i])->rspan
		    > row) {
			set_cell (table, table->row, i, table->cells [table->row - 1][i]);
			do_cspan (table, table->row, i + 1, table->cells [table->row -1][i]);
		}
}

static void
set_cell (HTMLTable *table, gint r, gint c, HTMLTableCell *cell)
{
	if (!table->cells [r][c]) {
		table->cells [r][c] = cell;
		html_table_cell_link (cell);
		HTML_OBJECT (cell)->parent = HTML_OBJECT (table);
	}
}

static void
alloc_cell (HTMLTable *table, gint r, gint c)
{
	if (c >= table->totalCols)
		inc_columns (table, c + 1 - table->totalCols);

	if (r >= table->totalRows)
		inc_rows (table, r + 1 - table->totalRows);
}


/* HTMLObject methods.  */

static gboolean
calc_size (HTMLObject *o,
	   HTMLPainter *painter)
{
	gint w;
	gint old_width, old_ascent, old_descent;
	gint pixel_size;
	gint r, c;
	gint i;
	gint available_width;
	HTMLTableCell *cell;
	HTMLTable *table;

	pixel_size = html_painter_get_pixel_size (painter);

	old_width = o->width;
	old_ascent = o->ascent;
	old_descent = o->descent;

	table = HTML_TABLE (o);

	
	if (o->change & HTML_CHANGE_MIN_WIDTH) {
		calc_col_info (table, painter);
		o->change &= ~HTML_CHANGE_MIN_WIDTH;
	}
	calc_column_widths (table, painter);

	/* If it doesn't fit... MAKE IT FIT!! */
	for (c = 0; c < table->totalCols; c++) {
		if (COLUMN_POS (table, c + 1) > o->max_width - pixel_size * table->border)
			COLUMN_POS (table, c + 1) = o->max_width - pixel_size * table->border;
	}

	g_array_set_size (table->columnOpt, table->totalCols + 1);
	for (i = 0; i < table->columnOpt->len; i++) 
		COLUMN_OPT (table, i) = COLUMN_POS (table, i);

	if (o->percent == 0 && ! (o->flags & HTML_OBJECT_FLAG_FIXEDWIDTH)) {
		o->width = COLUMN_OPT (table, table->totalCols) + pixel_size * table->border;
		available_width = o->max_width - 2 * pixel_size * table->border;
	} else {
		if (o->percent != 0) {
			/* The cast to `gdouble' is to avoid overflow (eg. when
			   printing).  */
			o->width = ((gdouble) o->max_width * o->percent) / 100;
		} else /* if (o->flags & HTML_OBJECT_FLAG_FIXEDWIDTH) */ {
			o->width = table->specified_width * pixel_size;
		}
		available_width = o->width - 2 * pixel_size * table->border;
	}
		
	/* Attempt to get sensible cell widths */
	optimize_cell_width (table, painter, available_width < 0 ? 0 : available_width);

	for (r = 0; r < table->totalRows; r++) {
		for (c = 0; c < table->totalCols; c++) {
			if ((cell = table->cells[r][c]) == 0)
				continue;
			if (c < table->totalCols - 1 &&
			    cell == table->cells [r][c + 1])
				continue;
			if (r < table->totalRows - 1 &&
			    cell == table->cells [r + 1][c])
				continue;

			w = (COLUMN_OPT (table, c + 1) - COLUMN_OPT (table, cell->col)
			     - pixel_size * (table->spacing + table->padding * 2));

			html_object_set_max_width (HTML_OBJECT (cell), painter, w);
			html_object_calc_size (HTML_OBJECT (cell), painter);
		}
	}

	if (table->caption) {
		g_print ("FIXME: Caption support\n");
	}

	/* We have all the cell sizes now, so calculate the vertical positions */
	calc_row_heights (table, painter);

	/* Set cell positions */
	for (r = 0; r < table->totalRows; r++) {
		gint cellHeight;
		
		HTML_OBJECT (table)->ascent = (ROW_HEIGHT (table, r + 1)
					       - pixel_size * (table->spacing));

		if (table->caption && table->capAlign == HTML_VALIGN_TOP) {
			g_print ("FIXME: caption support\n");
		}

		for (c = 0; c < table->totalCols; c++) {
			if ((cell = table->cells[r][c]) == 0)
				continue;
			if (c < table->totalCols - 1 &&
			    cell == table->cells[r][c + 1])
				continue;
			if (r < table->totalRows - 1 &&
			    cell == table->cells[r + 1][c])
				continue;

			HTML_OBJECT (cell)->x = COLUMN_OPT (table, cell->col);
			HTML_OBJECT (cell)->y = HTML_OBJECT (table)->ascent - HTML_OBJECT (cell)->descent;
			cellHeight            = (ROW_HEIGHT (table, r + 1) - ROW_HEIGHT (table, cell->row)
						 - pixel_size * (table->spacing));
			html_object_set_max_ascent (HTML_OBJECT (cell), painter, cellHeight);
		}

	}

	if (table->caption && table->capAlign == HTML_VALIGN_BOTTOM) {
		g_print ("FIXME: Caption support\n");
	}

	o->ascent = ROW_HEIGHT (table, table->totalRows) + pixel_size * table->border;
	o->width = COLUMN_OPT (table, table->totalCols) + pixel_size * table->border;
	
	if (table->caption) {
		g_print ("FIXME: Caption support\n");
	}

	/* FIXME Broken */
	if (o->width != old_width || o->ascent != old_ascent || o->descent != old_descent)
		return TRUE;

	return FALSE;
}

#define NEW_INDEX(l,h) ((l+h) >> 1);
#define ARR(i) g_array_index (a, gint, i-1)

static gint
bin_search_eq_or_lower_index (GArray *a, gint range, gint val, gint offset)
{
	gint l,h;
	gint i;

	l = offset ? offset + 1 : 1;
	h = range;
	i = NEW_INDEX (l, h);

	while (val != ARR (i) && l < h) {
		if (val < ARR (i))
			h = i - 1;
		else
			l = i + 1;
		i = NEW_INDEX (l, h);
	}

	if (val == ARR (i))
		return i - 1;

	if (val < ARR (l) && l > 1)
		return l - 2;
	else
		return l - 1;
}

static void
get_bounds (HTMLTable *table, gint x, gint y, gint width, gint height, gint *sc, gint *ec, gint *sr, gint *er)
{
	*sr = bin_search_eq_or_lower_index (table->rowHeights, table->totalRows + 1, y, 0);
	*er = MIN (bin_search_eq_or_lower_index (table->rowHeights, table->totalRows + 1, y + height, *sr) + 1,
		   table->totalRows);
	*sc = bin_search_eq_or_lower_index (table->columnOpt, table->totalCols + 1, x, 0);
	*ec = MIN (bin_search_eq_or_lower_index (table->columnOpt, table->totalCols + 1, x + width, *sc) + 1,
		   table->totalCols);
}

static void
draw (HTMLObject *o,
      HTMLPainter *p, 
      gint x, gint y,
      gint width, gint height,
      gint tx, gint ty)
{
	HTMLTableCell *cell;
	HTMLTable *table = HTML_TABLE (o);
	gint pixel_size;
	gint r, c, start_row, end_row, start_col, end_col;
	ArtIRect paint;

	html_object_calc_intersection (o, &paint, x, y, width, height);
	if (art_irect_empty (&paint))
		return;

	pixel_size = html_painter_get_pixel_size (p);
	
	tx += o->x;
	ty += o->y - o->ascent;

	/* Draw the cells */
	get_bounds (table, x, y, width, height, &start_col, &end_col, &start_row, &end_row);
	for (r = start_row; r < end_row; r++) {
		for (c = start_col; c < end_col; c++) {
			cell = table->cells[r][c];

			if (cell == NULL)
				continue;
			if (c < end_col - 1 && cell == table->cells [r][c + 1])
				continue;
			if (r < end_row - 1 && table->cells [r + 1][c] == cell)
				continue;

			html_object_draw (HTML_OBJECT (cell), p, 
					  x - o->x, y - o->y + o->ascent,
					  width,
					  height,
					  tx, ty);
		}
	}

	/* Draw the border */
	if (table->border > 0 && table->rowHeights->len > 0) {
		gint capOffset;

		capOffset = 0;

		if (table->caption && table->capAlign == HTML_VALIGN_TOP)
			g_print ("FIXME: Support captions\n");

		html_painter_draw_panel (p,  tx, ty + capOffset, 
					 HTML_OBJECT (table)->width,
					 ROW_HEIGHT (table, table->totalRows) +
					 pixel_size * table->border, GTK_HTML_ETCH_OUT,
					 pixel_size * table->border);
		
		/* Draw borders around each cell */
		for (r = start_row; r < end_row; r++) {
			for (c = start_col; c < end_col; c++) {
				if ((cell = table->cells[r][c]) == 0)
					continue;
				if (c < end_col - 1 &&
				    cell == table->cells[r][c + 1])
					continue;
				if (r < end_row - 1 &&
				    table->cells[r + 1][c] == cell)
					continue;

				html_painter_draw_panel (p,
							 tx + COLUMN_OPT (table, cell->col),
							 ty + ROW_HEIGHT (table, cell->row) + capOffset,
							 (COLUMN_OPT (table, c + 1)
							  - COLUMN_OPT (table, cell->col)
							  - pixel_size * table->spacing),
							 (ROW_HEIGHT (table, r + 1)
							  - ROW_HEIGHT (table, cell->row)
							  - pixel_size * table->spacing),
							 GTK_HTML_ETCH_IN, 1);
						      
			}
		}
	}
}

static gint
calc_min_width (HTMLObject *o,
		HTMLPainter *painter)
{
	calc_col_info (HTML_TABLE (o), painter);
	return HTML_TABLE(o)->_minWidth;
}

static gint
calc_preferred_width (HTMLObject *o,
		      HTMLPainter *painter)
{
	if (o->change & HTML_CHANGE_MIN_WIDTH) {
		calc_col_info (HTML_TABLE (o), painter);
		o->change &= ~HTML_CHANGE_MIN_WIDTH;
	}

	return HTML_TABLE (o)->_prefWidth;
}

static void
set_max_width (HTMLObject *o,
	       HTMLPainter *painter,
	       gint max_width)
{
	o->max_width = max_width;
}

static void
reset (HTMLObject *o)
{
	HTMLTable *table = HTML_TABLE (o);
	HTMLTableCell *cell;
	guint r, c;

	for (r = 0; r < table->totalRows; r++) {
		for (c = 0; c < table->totalCols; c++) {

			if ((cell = table->cells[r][c]) == 0)
				continue;
			if (c < table->totalCols - 1 &&
			    cell == table->cells[r][c + 1])
				continue;
			if (r < table->totalRows - 1 &&
			    cell == table->cells[r + 1][c])
				continue;
			
			html_object_reset (HTML_OBJECT (cell));
		}
	}
}

static HTMLAnchor *
find_anchor (HTMLObject *self, const char *name, gint *x, gint *y)
{
	HTMLTable *table;
	HTMLTableCell *cell;
	HTMLAnchor *anchor;
	unsigned int r, c;

	table = HTML_TABLE (self);

	*x += self->x;
	*y += self->y - self->ascent;

	for (r = 0; r < table->totalRows; r++) {
		for (c = 0; c < table->totalCols; c++) {
			if ((cell = table->cells[r][c]) == 0)
				continue;

			if (c < table->totalCols - 1
			    && cell == table->cells[r][c+1])
				continue;
			if (r < table->totalRows - 1
			    && table->cells[r+1][c] == cell)
				continue;

			anchor = html_object_find_anchor (HTML_OBJECT (cell), 
							  name, x, y);

			if (anchor != NULL)
				return anchor;
		}
	}
	*x -= self->x;
	*y -= self->y - self->ascent;

	return 0;
}

static HTMLObject *
check_point (HTMLObject *self,
	     HTMLPainter *painter,
	     gint x, gint y,
	     guint *offset_return,
	     gboolean for_cursor)
{
	HTMLTableCell *cell;
	HTMLObject *obj;
	HTMLTable *table;
	gint r, c, start_row, end_row, start_col, end_col;

	if (x < self->x || x > self->x + self->width
	    || y > self->y + self->descent || y < self->y - self->ascent)
		return NULL;

	table = HTML_TABLE (self);

	x -= self->x;
	y -= self->y - self->ascent;

	get_bounds (table, x, y, 1, 1, &start_col, &end_col, &start_row, &end_row);
	for (r = start_row; r < end_row; r++) {
		for (c = 0; c < table->totalCols; c++) {
			cell = table->cells[r][c];
			if (cell == NULL)
				continue;

			if (c < end_col - 1 && cell == table->cells[r][c+1])
				continue;
			if (r < end_row - 1 && table->cells[r+1][c] == cell)
				continue;

			obj = html_object_check_point (HTML_OBJECT (cell),
						       painter,
						       x, y,
						       offset_return,
						       for_cursor);
			if (obj != NULL)
				return obj;
		}
	}

	return NULL;
}

static gboolean
search (HTMLObject *obj, HTMLSearch *info)
{
	HTMLTable *table = HTML_TABLE (obj);
	HTMLTableCell *cell;
	HTMLObject    *cur = NULL;
	guint r, c, i, j;
	gboolean next = FALSE;

	/* search_next? */
	if (html_search_child_on_stack (info, obj)) {
		cur  = html_search_pop (info);
		next = TRUE;
	}

	if (info->forward) {
		for (r = 0; r < table->totalRows; r++) {
			for (c = 0; c < table->totalCols; c++) {

				if ((cell = table->cells[r][c]) == 0)
					continue;
				if (c < table->totalCols - 1 &&
				    cell == table->cells[r][c + 1])
					continue;
				if (r < table->totalRows - 1 &&
				    cell == table->cells[r + 1][c])
					continue;

				if (next && cur) {
					if (HTML_OBJECT (cell) == cur) {
						cur = NULL;
					}
					continue;
				}

				html_search_push (info, HTML_OBJECT (cell));
				if (html_object_search (HTML_OBJECT (cell), info)) {
					return TRUE;
				}
				html_search_pop (info);
			}
		}
	} else {
		for (i = 0, r = table->totalRows - 1; i < table->totalRows; i++, r--) {
			for (j = 0, c = table->totalCols - 1; j < table->totalCols; j++, c--) {

				if ((cell = table->cells[r][c]) == 0)
					continue;
				if (c < table->totalCols - 1 &&
				    cell == table->cells[r][c + 1])
					continue;
				if (r < table->totalRows - 1 &&
				    cell == table->cells[r + 1][c])
					continue;

				if (next && cur) {
					if (HTML_OBJECT (cell) == cur) {
						cur = NULL;
					}
					continue;
				}

				html_search_push (info, HTML_OBJECT (cell));
				if (html_object_search (HTML_OBJECT (cell), info)) {
					return TRUE;
				}
				html_search_pop (info);
			}
		}
	}

	if (next) {
		return html_search_next_parent (info);
	}

	return FALSE;
}

static HTMLFitType
fit_line (HTMLObject *o,
	  HTMLPainter *painter,
	  gboolean start_of_line,
	  gboolean first_run,
	  gint width_left)
{
	return HTML_FIT_COMPLETE;
}

static void
append_selection_string (HTMLObject *self,
			 GString *buffer)
{
	HTMLTable *table;
	HTMLTableCell *cell;
	gboolean tab;
	gint r, c, len, rlen, tabs;

	table = HTML_TABLE (self);
	for (r = 0; r < table->totalRows; r++) {
		tab = FALSE;
		tabs = 0;
		rlen = buffer->len;
		for (c = 0; c < table->totalCols; c++) {
			if ((cell = table->cells[r][c]) == 0)
				continue;
			if (c < table->totalCols - 1 &&
			    cell == table->cells[r][c + 1])
				continue;
			if (r < table->totalRows - 1 &&
			    table->cells[r + 1][c] == cell)
				continue;
			if (tab) {
				g_string_append_c (buffer, '\t');
				tabs++;
			}
			len = buffer->len;
			html_object_append_selection_string (HTML_OBJECT (cell), buffer);
			/* remove last \n from added text */
			if (buffer->len != len && buffer->str [buffer->len-1] == '\n')
				g_string_truncate (buffer, buffer->len - 1);
			tab = TRUE;
		}
		if (rlen + tabs == buffer->len)
			g_string_truncate (buffer, rlen);
		else if (r + 1 < table->totalRows)
			g_string_append_c (buffer, '\n');
	}
}

static HTMLObject *
head (HTMLObject *self)
{
	HTMLTable *table;
	gint r, c;

	table = HTML_TABLE (self);
	for (r = 0; r < table->totalRows; r++)
		for (c = 0; c < table->totalCols; c++) {
			if (invalid_cell (table, r, c))
				continue;
			return HTML_OBJECT (table->cells [r][c]);
		}
	return NULL;
}

static HTMLObject *
tail (HTMLObject *self)
{
	HTMLTable *table;
	gint r, c;

	table = HTML_TABLE (self);
	for (r = table->totalRows - 1; r >= 0; r--)
		for (c = table->totalCols - 1; c >= 0; c--) {
			if (invalid_cell (table, r, c))
				continue;
			return HTML_OBJECT (table->cells [r][c]);
		}
	return NULL;
}

static HTMLObject *
next (HTMLObject *self, HTMLObject *child)
{
	HTMLTable *table;
	gint r, c;

	table = HTML_TABLE (self);
	r = HTML_TABLE_CELL (child)->row;
	c = HTML_TABLE_CELL (child)->col;
	for (c++; r < table->totalRows; r++) {
		for (; c < table->totalCols; c++) {
			if (invalid_cell (table, r, c))
				continue;
			return HTML_OBJECT (table->cells [r][c]);
		}
		c = 0;
	}
	return NULL;
}

static HTMLObject *
prev (HTMLObject *self, HTMLObject *child)
{
	HTMLTable *table;
	gint r, c;

	table = HTML_TABLE (self);
	r = HTML_TABLE_CELL (child)->row;
	c = HTML_TABLE_CELL (child)->col;
	for (c--; r >= 0; r--) {
		for (; c >=0; c--) {
			if (invalid_cell (table, r, c))
				continue;
			return HTML_OBJECT (table->cells [r][c]);
		}
		c = table->totalCols-1;
	}
	return NULL;
}


void
html_table_type_init (void)
{
	html_table_class_init (&html_table_class, HTML_TYPE_TABLE, sizeof (HTMLTable));
}

void
html_table_class_init (HTMLTableClass *klass,
		       HTMLType type,
		       guint object_size)
{
	HTMLObjectClass *object_class;

	object_class = HTML_OBJECT_CLASS (klass);

	html_object_class_init (object_class, type, object_size);

	object_class->copy = copy;
	object_class->calc_size = calc_size;
	object_class->draw = draw;
	object_class->destroy = destroy;
	object_class->calc_min_width = calc_min_width;
	object_class->calc_preferred_width = calc_preferred_width;
	object_class->set_max_width = set_max_width;
	object_class->reset = reset;
	object_class->check_point = check_point;
	object_class->find_anchor = find_anchor;
	object_class->is_container = is_container;
	object_class->forall = forall;
	object_class->search = search;
	object_class->fit_line = fit_line;
	object_class->append_selection_string = append_selection_string;
	object_class->head = head;
	object_class->tail = tail;
	object_class->next = next;
	object_class->prev = prev;

	parent_class = &html_object_class;
}

void
html_table_init (HTMLTable *table,
		 HTMLTableClass *klass,
		 gint width, gint percent,
		 gint padding, gint spacing, gint border)
{
	HTMLObject *object;
	gint r;

	object = HTML_OBJECT (table);

	html_object_init (object, HTML_OBJECT_CLASS (klass));

	object->percent = percent;

	table->specified_width = width;
	if (width == 0)
		object->flags &= ~ HTML_OBJECT_FLAG_FIXEDWIDTH;
	else
		object->flags |= HTML_OBJECT_FLAG_FIXEDWIDTH;

	table->_minWidth = 0;
	table->_prefWidth = 0;

	table->padding = padding;
	table->spacing = spacing;
	table->border = border;
	table->caption = NULL;
	table->capAlign = HTML_VALIGN_TOP;

	table->row = 0;
	table->col = 0;

	table->totalCols = 1; /* this should be expanded to the maximum number
				 of cols by the first row parsed */
	table->totalRows = 1;
	table->allocRows = 5; /* allocate five rows initially */

	table->cells = g_new0 (HTMLTableCell **, table->allocRows);
	for (r = 0; r < table->allocRows; r++)
		table->cells[r] = g_new0 (HTMLTableCell *, table->totalCols);;
	
	table->totalColumnInfos = 0;

	table->colInfo = g_array_new (FALSE, TRUE, sizeof (ColumnInfo));
	table->colType = g_array_new (FALSE, FALSE, sizeof (ColumnType));
	table->columnPos = g_array_new (FALSE, FALSE, sizeof (gint));
	table->columnPrefPos = g_array_new (FALSE, FALSE, sizeof (gint));
	table->colSpan = g_array_new (FALSE, FALSE, sizeof (gint));
	table->columnOpt = g_array_new (FALSE, FALSE, sizeof (gint));
	table->rowHeights = g_array_new (FALSE, FALSE, sizeof (gint));
}

HTMLObject *
html_table_new (gint width, gint percent,
		gint padding, gint spacing, gint border)
{
	HTMLTable *table;

	table = g_new (HTMLTable, 1);
	html_table_init (table, &html_table_class,
			 width, percent, padding, spacing, border);

	return HTML_OBJECT (table);
}



void
html_table_add_cell (HTMLTable *table, HTMLTableCell *cell)
{
	alloc_cell (table, table->row, table->col);
	prev_col_do_cspan (table, table->row);

	/* look for first free cell */
	while (table->cells [table->row][table->col] && table->col < table->totalCols)
		table->col++;

	alloc_cell (table, table->row, table->col);
	set_cell (table, table->row, table->col, cell);
	html_table_cell_set_position (cell, table->row, table->col);
	do_cspan(table, table->row, table->col, cell);
}

void
html_table_start_row (HTMLTable *table)
{
	table->col = 0;
}

void
html_table_end_row (HTMLTable *table)
{
	gint i;

	for (i=0; i < table->totalCols; i++)
		if (table->cells [table->row][table->col]) {
			table->row++;
			break;
		}
}

void
html_table_end_table (HTMLTable *table)
{
}

gint
html_table_add_col_info (HTMLTable *table, gint startCol, gint colSpan,
			 gint minSize, gint prefSize, gint maxSize,
			 ColumnType coltype)
{
	gint indx;
	
	/* Is there already some info present? */
	for (indx = 0; indx < table->totalColumnInfos; indx++) {
		if ((COLUMN_INFO (table, indx).startCol == startCol) &&
		    (COLUMN_INFO (table, indx).colSpan == colSpan))
			break;
	}
	if (indx == table->totalColumnInfos) {
		/* No colInfo present, allocate some */
		table->totalColumnInfos++;
		if (table->totalColumnInfos >= table->colInfo->len)
			g_array_set_size (table->colInfo, table->colInfo->len + table->totalCols);

		COLUMN_INFO (table, indx).startCol = startCol;
		COLUMN_INFO (table, indx).colSpan = colSpan;
		COLUMN_INFO (table, indx).minSize = minSize;
		COLUMN_INFO (table, indx).prefSize = prefSize;
		COLUMN_INFO (table, indx).maxSize = maxSize;
		COLUMN_INFO (table, indx).colType = coltype;

	} else {
		if (minSize > (COLUMN_INFO (table, indx).minSize))
			COLUMN_INFO (table, indx).minSize = minSize;

		/* COLUMN_TYPE_FIXED < COLUMN_TYPE_PERCENT < COLUMN_TYPE_VARIABLE */
		if (coltype < COLUMN_INFO (table, indx).colType) {
			COLUMN_INFO (table, indx).prefSize = prefSize;
		}
		else if (coltype == COLUMN_INFO (table, indx).colType) {
			if (prefSize > COLUMN_INFO (table, indx).prefSize)
				COLUMN_INFO (table, indx).prefSize = prefSize;
		}
	}

	return indx; /* Return the ColumnInfo Index */
}
