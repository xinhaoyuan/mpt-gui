(with (GenFilterByTag
       FilterByArtist
       FilterByAlbum
       FilterByArtistAndAlbum
       )

      (set! GenFilterByTag
            (lambda (tag)
              (lambda (song-list)
                (with (process)
                      (set! process
                            (lambda (cur)
                              (if cur
                                  (begin
                                    ("SubviewPut" (#car cur) ("MetaGet" (#car cur) tag))
                                    (process (#cdr cur))))
                              ))
                      (process song-list)))))
      
      (set! FilterByArtist (GenFilterByTag "Artist"))
      (set! FilterByAlbum  (GenFilterByTag "Album"))
      (set! FilterByArtistAndAlbum
            (lambda (song-list)
              (FilterByArtist song-list)
              ("SubviewDefaultFilterSet" "ByAlbum")))

      ("FilterExport" "ByArtist" FilterByArtist)
      ("FilterExport" "ByAlbum"  FilterByAlbum)
      ("FilterExport" "ByArtistAndAlbum" FilterByArtistAndAlbum)

      )
