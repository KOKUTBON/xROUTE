/**
 * @file parsers.c
 * @brief Zero-dependency, robust XML and CSV map parsers and serializers.
 */

#include "xroute/parser.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Helper to trim leading/trailing whitespace */
static char *trim(char *s) {
  if (!s)
    return NULL;
  while (isspace((unsigned char)*s))
    s++;
  if (*s == 0)
    return s;
  char *end = s + strlen(s) - 1;
  while (end > s && isspace((unsigned char)*end))
    end--;
  end[1] = '\0';
  return s;
}

/* Parse directional string ("sx", "dx", "up", "dw") to direction flag */
static uint8_t parse_direction_string(const char *str) {
  if (!str)
    return XROUTE_DIR_NONE;
  if (strstr(str, "sx"))
    return XROUTE_DIR_LEFT;
  if (strstr(str, "dx"))
    return XROUTE_DIR_RIGHT;
  if (strstr(str, "up"))
    return XROUTE_DIR_UP;
  if (strstr(str, "dw"))
    return XROUTE_DIR_DOWN;
  return XROUTE_DIR_NONE;
}

/* Extract attribute value: finds attr_name="value" or attr_name='value' */
static bool get_xml_attribute(const char *tag_str, const char *attr_name,
                              char *out_val, size_t max_len) {
  char pattern[128];
  snprintf(pattern, sizeof(pattern), "%s=", attr_name);
  const char *pos = strstr(tag_str, pattern);
  if (!pos)
    return false;

  pos += strlen(pattern);
  while (*pos == ' ' || *pos == '\t')
    pos++;

  char quote = *pos;
  if (quote != '"' && quote != '\'')
    return false;
  pos++;

  const char *closing = strchr(pos, quote);
  if (!closing)
    return false;

  size_t len = (size_t)(closing - pos);
  if (len >= max_len)
    len = max_len - 1;
  strncpy(out_val, pos, len);
  out_val[len] = '\0';
  return true;
}

xroute_status_t xroute_map_load_xml_string(const char *xml_content,
                                           xroute_map_t **out_map) {
  if (!xml_content || !out_map)
    return XROUTE_ERR_INVALID_PARAM;
  *out_map = NULL;

  /*
   * Look for <Magazzino or <Deposito tag to find dimensions
   */
  const char *root = strstr(xml_content, "<Magazzino");
  if (!root)
    root = strstr(xml_content, "<Deposito");
  if (!root)
    return XROUTE_ERR_PARSE_FAILED;

  /* Check if dimensions are in <Deposito> inside <Magazzino> */
  const char *deposito = strstr(root, "<Deposito");
  const char *dim_tag =
      (deposito && deposito < strstr(root, ">")) ? deposito : root;
  if (deposito && strstr(root, "<Deposito")) {
    dim_tag = deposito;
  }

  char buf[64];
  int rows = 0, cols = 0, dim_col = 350, dim_row = 200;

  if (get_xml_attribute(dim_tag, "numRighe", buf, sizeof(buf))) {
    rows = atoi(buf);
  } else if (get_xml_attribute(root, "numRighe", buf, sizeof(buf))) {
    rows = atoi(buf);
  }

  if (get_xml_attribute(dim_tag, "numColonne", buf, sizeof(buf))) {
    cols = atoi(buf);
  } else if (get_xml_attribute(root, "numColonne", buf, sizeof(buf))) {
    cols = atoi(buf);
  }

  if (get_xml_attribute(dim_tag, "dimColonne", buf, sizeof(buf))) {
    dim_col = atoi(buf);
  } else if (get_xml_attribute(root, "dimColonne", buf, sizeof(buf))) {
    dim_col = atoi(buf);
  }

  if (get_xml_attribute(dim_tag, "dimRighe", buf, sizeof(buf))) {
    dim_row = atoi(buf);
  } else if (get_xml_attribute(root, "dimRighe", buf, sizeof(buf))) {
    dim_row = atoi(buf);
  }

  if (rows <= 0 || cols <= 0) {
    return XROUTE_ERR_PARSE_FAILED;
  }

  xroute_map_t *map = xroute_map_create(rows, cols, dim_col, dim_row);
  if (!map)
    return XROUTE_ERR_NO_MEMORY;

  /* Iterate through all <Cella ...> ... </Cella> blocks */
  const char *curr = root;
  while ((curr = strstr(curr, "<Cella")) != NULL) {
    const char *tag_end = strchr(curr, '>');
    if (!tag_end)
      break;

    /* Extract opening tag attributes */
    size_t tag_len = (size_t)(tag_end - curr + 1);
    char *tag_header = (char *)malloc(tag_len + 1);
    if (!tag_header) {
      xroute_map_free(map);
      return XROUTE_ERR_NO_MEMORY;
    }
    strncpy(tag_header, curr, tag_len);
    tag_header[tag_len] = '\0';

    int x = -1, y = -1, tipo = 0, corsia = 0, campata = 0;
    if (get_xml_attribute(tag_header, "x", buf, sizeof(buf)))
      x = atoi(buf);
    if (get_xml_attribute(tag_header, "y", buf, sizeof(buf)))
      y = atoi(buf);
    if (get_xml_attribute(tag_header, "Tipo", buf, sizeof(buf)))
      tipo = atoi(buf);
    if (get_xml_attribute(tag_header, "Corsia", buf, sizeof(buf)))
      corsia = atoi(buf);
    if (get_xml_attribute(tag_header, "Campata", buf, sizeof(buf)))
      campata = atoi(buf);

    free(tag_header);

    /* Find body until </Cella> or check self-closing tag */
    uint8_t access_sides = XROUTE_DIR_NONE;
    uint8_t allowed_moves = XROUTE_DIR_NONE;
    bool has_explicit_moves = false;

    const char *cell_close = strstr(curr, "</Cella>");
    const char *next_cell = strstr(curr + 6, "<Cella");
    if (cell_close && (!next_cell || cell_close < next_cell)) {
      /* Scan child tags inside this cell */
      const char *p = tag_end + 1;
      while (p < cell_close) {
        const char *tag_start = strchr(p, '<');
        if (!tag_start || tag_start >= cell_close)
          break;
        if (strncmp(tag_start, "<LatoAccessibile>", 17) == 0) {
          const char *val_start = tag_start + 17;
          const char *val_end = strstr(val_start, "</LatoAccessibile>");
          if (val_end && val_end <= cell_close) {
            char dir_str[16];
            size_t dlen = (size_t)(val_end - val_start);
            if (dlen >= sizeof(dir_str))
              dlen = sizeof(dir_str) - 1;
            strncpy(dir_str, val_start, dlen);
            dir_str[dlen] = '\0';
            access_sides |= parse_direction_string(dir_str);
            p = val_end + 18;
            continue;
          }
        } else if (strncmp(tag_start, "<SpostamentoPermesso>", 21) == 0) {
          const char *val_start = tag_start + 21;
          const char *val_end = strstr(val_start, "</SpostamentoPermesso>");
          if (val_end && val_end <= cell_close) {
            char dir_str[16];
            size_t dlen = (size_t)(val_end - val_start);
            if (dlen >= sizeof(dir_str))
              dlen = sizeof(dir_str) - 1;
            strncpy(dir_str, val_start, dlen);
            dir_str[dlen] = '\0';
            allowed_moves |= parse_direction_string(dir_str);
            has_explicit_moves = true;
            p = val_end + 22;
            continue;
          }
        }
        p = tag_start + 1;
      }
      curr = cell_close + 8;
    } else {
      curr = tag_end + 1;
    }

    if (x >= 0 && x < cols && y >= 0 && y < rows) {
      xroute_cell_type_t cell_type = XROUTE_CELL_TRANSIT;
      if (tipo == 1) {
        cell_type = XROUTE_CELL_RACK_BLOCKED;
        xroute_map_add_bay(map, x, y, cell_type, corsia, campata, access_sides,
                           XROUTE_DIR_NONE);
      } else if (tipo == 2) {
        cell_type = XROUTE_CELL_RACK_TRANSIT;
        if (!has_explicit_moves)
          allowed_moves = XROUTE_DIR_ALL;
        xroute_map_add_bay(map, x, y, cell_type, corsia, campata, access_sides,
                           allowed_moves);
      } else if (tipo == 3) {
        cell_type = XROUTE_CELL_RESTRICTED;
        if (!has_explicit_moves)
          allowed_moves = XROUTE_DIR_ALL;
        xroute_map_set_cell(map, x, y, cell_type, allowed_moves);
      } else if (tipo == 4) {
        cell_type = XROUTE_CELL_OBSTACLE;
        xroute_map_set_cell(map, x, y, cell_type, XROUTE_DIR_NONE);
      }
    }
  }

  *out_map = map;
  return XROUTE_SUCCESS;
}

xroute_status_t xroute_map_load_xml(const char *filepath,
                                    xroute_map_t **out_map) {
  if (!filepath || !out_map)
    return XROUTE_ERR_INVALID_PARAM;
  *out_map = NULL;

  FILE *fp = fopen(filepath, "rb");
  if (!fp)
    return XROUTE_ERR_FILE_NOT_FOUND;

  fseek(fp, 0, SEEK_END);
  long size = ftell(fp);
  fseek(fp, 0, SEEK_SET);

  if (size <= 0) {
    fclose(fp);
    return XROUTE_ERR_PARSE_FAILED;
  }

  char *buffer = (char *)malloc(size + 1);
  if (!buffer) {
    fclose(fp);
    return XROUTE_ERR_NO_MEMORY;
  }

  size_t read_bytes = fread(buffer, 1, size, fp);
  fclose(fp);
  buffer[read_bytes] = '\0';

  xroute_status_t st = xroute_map_load_xml_string(buffer, out_map);
  free(buffer);
  return st;
}

xroute_status_t xroute_map_load_csv(const char *filepath, int cell_width_cm,
                                    int cell_height_cm,
                                    xroute_map_t **out_map) {
  if (!filepath || !out_map)
    return XROUTE_ERR_INVALID_PARAM;
  *out_map = NULL;

  FILE *fp = fopen(filepath, "r");
  if (!fp)
    return XROUTE_ERR_FILE_NOT_FOUND;

  char line[4096];
  if (!fgets(line, sizeof(line), fp)) {
    fclose(fp);
    return XROUTE_ERR_PARSE_FAILED;
  }

  /*
   * Line 1: numRighe,numColonne,,
   */
  int rows = 0, cols = 0;
  if (sscanf(line, "%d,%d", &rows, &cols) < 2 || rows <= 0 || cols <= 0) {
    fclose(fp);
    return XROUTE_ERR_PARSE_FAILED;
  }

  /* Line 2 is blank / commas */
  if (!fgets(line, sizeof(line), fp)) {
    fclose(fp);
    return XROUTE_ERR_PARSE_FAILED;
  }

  if (cell_width_cm <= 0)
    cell_width_cm = 350;
  if (cell_height_cm <= 0)
    cell_height_cm = 200;

  xroute_map_t *map =
      xroute_map_create(rows, cols, cell_width_cm, cell_height_cm);
  if (!map) {
    fclose(fp);
    return XROUTE_ERR_NO_MEMORY;
  }

  /* Allocate matrix to store parsed CSV raw values for neighbor checking */
  int *raw_matrix = (int *)malloc(rows * (cols + 1) * sizeof(int));
  if (!raw_matrix) {
    xroute_map_free(map);
    fclose(fp);
    return XROUTE_ERR_NO_MEMORY;
  }
  memset(raw_matrix, 0, rows * (cols + 1) * sizeof(int));

  for (int r = 0; r < rows; ++r) {
    if (!fgets(line, sizeof(line), fp))
      break;

    char *token = line;
    int col_idx = 0;

    while (token && col_idx <= cols) {
      char *next = strchr(token, ',');
      if (next)
        *next = '\0';

      char *val = trim(token);
      if (strcmp(val, "x") == 0 || strcmp(val, "X") == 0) {
        raw_matrix[r * (cols + 1) + col_idx] = -1; /* Obstacle */
      } else if (strlen(val) > 0) {
        raw_matrix[r * (cols + 1) + col_idx] = atoi(val);
      } else {
        raw_matrix[r * (cols + 1) + col_idx] = 0; /* Transit corridor */
      }

      token = next ? next + 1 : NULL;
      col_idx++;
    }
  }
  fclose(fp);

  /*
   * Build map topology from raw matrix according to Docs/MAP_FORMATS.md:
   * raw_matrix[r, 0] = corsia
   * raw_matrix[r, c] for c in 1..cols:
   * -1 -> Obstacle (XROUTE_CELL_OBSTACLE)
   *  0 -> Transit cell (XROUTE_CELL_TRANSIT)
   * >0 -> Type 1 rack (campata = val, corsia = raw_matrix[r, 0])
   * Accessible sides are automatically determined if neighbor is transit (0).
   */
  for (int r = 0; r < rows; ++r) {
    int corsia = raw_matrix[r * (cols + 1) + 0];
    for (int c = 1; c <= cols; ++c) {
      int map_x = c - 1; /* 0-based coordinate */
      int map_y = r;
      int cell_val = raw_matrix[r * (cols + 1) + c];

      if (cell_val == -1) {
        xroute_map_set_cell(map, map_x, map_y, XROUTE_CELL_OBSTACLE,
                            XROUTE_DIR_NONE);
      } else if (cell_val > 0) {
        int campata = cell_val;
        uint8_t access = XROUTE_DIR_NONE;

        /* Top neighbor */
        if (r > 0 && raw_matrix[(r - 1) * (cols + 1) + c] == 0) {
          access |= XROUTE_DIR_UP;
        }
        /* Bottom neighbor */
        if (r < rows - 1 && raw_matrix[(r + 1) * (cols + 1) + c] == 0) {
          access |= XROUTE_DIR_DOWN;
        }
        /* Right neighbor */
        if (c < cols && raw_matrix[r * (cols + 1) + (c + 1)] == 0) {
          access |= XROUTE_DIR_RIGHT;
        }
        /* Left neighbor */
        if (c > 1 && raw_matrix[r * (cols + 1) + (c - 1)] == 0) {
          access |= XROUTE_DIR_LEFT;
        }

        xroute_map_add_bay(map, map_x, map_y, XROUTE_CELL_RACK_BLOCKED, corsia,
                           campata, access, XROUTE_DIR_NONE);
      } else {
        xroute_map_set_cell(map, map_x, map_y, XROUTE_CELL_TRANSIT,
                            XROUTE_DIR_ALL);
      }
    }
  }

  free(raw_matrix);
  *out_map = map;
  return XROUTE_SUCCESS;
}

xroute_status_t xroute_map_save_xml(const xroute_map_t *map,
                                    const char *filepath) {
  if (!map || !filepath)
    return XROUTE_ERR_INVALID_PARAM;

  FILE *fp = fopen(filepath, "w");
  if (!fp)
    return XROUTE_ERR_IO;

  int rows = xroute_map_get_rows(map);
  int cols = xroute_map_get_cols(map);
  int dim_col = xroute_map_get_cell_width_cm(map);
  int dim_row = xroute_map_get_cell_height_cm(map);

  fprintf(fp, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
  fprintf(fp,
          "<Magazzino numRighe=\"%d\" numColonne=\"%d\" dimColonne=\"%d\" "
          "dimRighe=\"%d\">\n",
          rows, cols, dim_col, dim_row);

  int bay_count = 0;
  const xroute_bay_t *bays = xroute_map_get_all_bays(map, &bay_count);

  /* Output all bays */
  for (int i = 0; i < bay_count; ++i) {
    const xroute_bay_t *b = &bays[i];
    int tipo = (b->type == XROUTE_CELL_RACK_BLOCKED) ? 1 : 2;
    fprintf(fp,
            "\t<Cella x=\"%d\" y=\"%d\" Tipo=\"%d\" Corsia=\"%d\" "
            "Campata=\"%d\">\n",
            b->x, b->y, tipo, b->corsia, b->campata);

    if (b->accessible_sides & XROUTE_DIR_LEFT)
      fprintf(fp, "\t\t<LatoAccessibile>sx</LatoAccessibile>\n");
    if (b->accessible_sides & XROUTE_DIR_UP)
      fprintf(fp, "\t\t<LatoAccessibile>up</LatoAccessibile>\n");
    if (b->accessible_sides & XROUTE_DIR_RIGHT)
      fprintf(fp, "\t\t<LatoAccessibile>dx</LatoAccessibile>\n");
    if (b->accessible_sides & XROUTE_DIR_DOWN)
      fprintf(fp, "\t\t<LatoAccessibile>dw</LatoAccessibile>\n");

    if (tipo == 2) {
      if (b->allowed_movements & XROUTE_DIR_LEFT)
        fprintf(fp, "\t\t<SpostamentoPermesso>sx</SpostamentoPermesso>\n");
      if (b->allowed_movements & XROUTE_DIR_UP)
        fprintf(fp, "\t\t<SpostamentoPermesso>up</SpostamentoPermesso>\n");
      if (b->allowed_movements & XROUTE_DIR_RIGHT)
        fprintf(fp, "\t\t<SpostamentoPermesso>dx</SpostamentoPermesso>\n");
      if (b->allowed_movements & XROUTE_DIR_DOWN)
        fprintf(fp, "\t\t<SpostamentoPermesso>dw</SpostamentoPermesso>\n");
    }

    fprintf(fp, "\t</Cella>\n");
  }

  /* Output obstacles and restricted cells */
  for (int y = 0; y < rows; ++y) {
    for (int x = 0; x < cols; ++x) {
      xroute_cell_type_t t = xroute_map_get_cell_type(map, x, y);
      if (t == XROUTE_CELL_OBSTACLE) {
        fprintf(fp, "\t<Cella x=\"%d\" y=\"%d\" Tipo=\"4\"></Cella>\n", x, y);
      } else if (t == XROUTE_CELL_RESTRICTED) {
        uint8_t m = xroute_map_get_allowed_movements(map, x, y);
        fprintf(fp, "\t<Cella x=\"%d\" y=\"%d\" Tipo=\"3\">\n", x, y);
        if (m & XROUTE_DIR_LEFT)
          fprintf(fp, "\t\t<SpostamentoPermesso>sx</SpostamentoPermesso>\n");
        if (m & XROUTE_DIR_UP)
          fprintf(fp, "\t\t<SpostamentoPermesso>up</SpostamentoPermesso>\n");
        if (m & XROUTE_DIR_RIGHT)
          fprintf(fp, "\t\t<SpostamentoPermesso>dx</SpostamentoPermesso>\n");
        if (m & XROUTE_DIR_DOWN)
          fprintf(fp, "\t\t<SpostamentoPermesso>dw</SpostamentoPermesso>\n");
        fprintf(fp, "\t</Cella>\n");
      }
    }
  }

  fprintf(fp, "</Magazzino>\n");
  fclose(fp);
  return XROUTE_SUCCESS;
}

xroute_status_t xroute_csv_to_xml(const char *csv_path, const char *xml_path,
                                  int cell_width_cm, int cell_height_cm) {
  xroute_map_t *map = NULL;
  xroute_status_t st =
      xroute_map_load_csv(csv_path, cell_width_cm, cell_height_cm, &map);
  if (st != XROUTE_SUCCESS)
    return st;

  st = xroute_map_save_xml(map, xml_path);
  xroute_map_free(map);
  return st;
}
