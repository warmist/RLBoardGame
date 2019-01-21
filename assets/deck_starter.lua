deck={}

deck.strike={
	name="Strike",
	cost=2,
	description="Perform an attack with a very big sword",
	tags={"attack 3","range 0"},
	use=function ( card,game,data )
		game.damage(data.target,card.attack)
	end
}
deck.push={
	name="Push",
	cost=1,
	description="Force a move. Deal damage if can't move",
	tags={"attack 1","range 0","distance 3"},
	use=function ( card,game,data )
		--figure out the direction of push
		local dir=data.target.pos-game.player.pos
		dir=dir/dir:lenght()
		local start=data.target.pos
		--raycast (i.e. like going straight in that direction)
		local path=game.raycast(start+dir,dir,card.distance-1,data.target)
		--move the unit
		game.move(data.target,path)
		--then if the unit could not move ALL the way, hurt it
		if #path<card.distance-1 then
			game.damage(data.target,card.attack)
		end
	end
}
deck.move={
	name="Run",
	cost=1,
	description="Run to the target",
	tags={"generated","range 5","move"},
	use=function (card,game,data)
		game.move(game.player,data.path)
	end
}
return deck